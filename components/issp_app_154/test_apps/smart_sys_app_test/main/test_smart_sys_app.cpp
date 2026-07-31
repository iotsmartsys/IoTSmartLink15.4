// Automated coverage for iotsmartsys::SmartSysApp: configuration validation
// (SMARTAPP-AC-001, AC-004, AC-004A, AC-005), states, initialization order,
// repeated setup(), injected failures and rollback (SMARTAPP-AC-006 to
// AC-013). Every SmartSysApp instance here is built with
// SmartSysApp::SetupHooks, replacing the platform/network/device/executor
// steps with fakes, so nothing in this file ever calls NVS, GPIO drivers or
// radio APIs. That is also why this app targets esp32c3 and runs under
// QEMU (see components/issp_app_154/CMakeLists.txt: the hardware-only
// smart_sys_app_hardware.cpp, and the production single-argument
// SmartSysApp constructor it defines, are not even compiled for a target
// without an IEEE 802.15.4 radio) instead of depending on real hardware.

#include <array>
#include <cstddef>

#include "SmartSysApp.h"
#include "issp_limits.hpp"
#include "unity.h"

using namespace iotsmartsys;

namespace
{

app::SwitchConfig makeSwitchConfig(std::uint8_t endpointId, std::uint8_t eventType)
{
    return {
        .pin = GPIO_NUM_2,
        .activeHigh = true,
        .initialState = false,
        .reportOnStart = false,
        .endpointId = endpointId,
        .eventType = eventType,
    };
}

enum class Step
{
    InitializePlatform,
    InitializeNetwork,
    RegisterCapability,
    StartDevice,
    StartReportExecutor,
    RollbackTransport,
};

// Records every hook invocation and lets each test script the AppResult
// each step returns, without touching NVS, GPIO or radio APIs.
struct FakeScenario
{
    AppResult initializePlatformResult = AppResult::Ok;
    AppResult initializeNetworkResult = AppResult::Ok;
    AppResult registerCapabilityResult = AppResult::Ok;
    AppResult startDeviceResult = AppResult::Ok;
    AppResult startReportExecutorResult = AppResult::Ok;

    static constexpr std::size_t kMaxCalls = 16;
    std::array<Step, kMaxCalls> callOrder{};
    std::size_t callCount = 0;
    std::size_t registerCapabilityCalls = 0;
    std::size_t rollbackCalls = 0;

    void record(Step step)
    {
        if (callCount < kMaxCalls)
        {
            callOrder[callCount] = step;
        }
        ++callCount;
    }
};

AppResult fakeInitializePlatform(void *context)
{
    auto *scenario = static_cast<FakeScenario *>(context);
    scenario->record(Step::InitializePlatform);
    return scenario->initializePlatformResult;
}

AppResult fakeInitializeNetwork(void *context)
{
    auto *scenario = static_cast<FakeScenario *>(context);
    scenario->record(Step::InitializeNetwork);
    return scenario->initializeNetworkResult;
}

AppResult fakeRegisterCapability(void *context, std::size_t /*index*/)
{
    auto *scenario = static_cast<FakeScenario *>(context);
    scenario->record(Step::RegisterCapability);
    ++scenario->registerCapabilityCalls;
    return scenario->registerCapabilityResult;
}

AppResult fakeStartDevice(void *context)
{
    auto *scenario = static_cast<FakeScenario *>(context);
    scenario->record(Step::StartDevice);
    return scenario->startDeviceResult;
}

AppResult fakeStartReportExecutor(void *context)
{
    auto *scenario = static_cast<FakeScenario *>(context);
    scenario->record(Step::StartReportExecutor);
    return scenario->startReportExecutorResult;
}

void fakeRollbackTransport(void *context)
{
    auto *scenario = static_cast<FakeScenario *>(context);
    scenario->record(Step::RollbackTransport);
    ++scenario->rollbackCalls;
}

SmartSysApp::SetupHooks makeHooks(FakeScenario &scenario)
{
    return SmartSysApp::SetupHooks{
        &fakeInitializePlatform,
        &fakeInitializeNetwork,
        &fakeRegisterCapability,
        &fakeStartDevice,
        &fakeStartReportExecutor,
        &fakeRollbackTransport,
        &scenario,
    };
}

} // namespace

// --- configuration validation (never reaches setup()) ---

TEST_CASE("construction with a valid config starts Configuring", "[smart_sys_app]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 0x00000001}, makeHooks(scenario));
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Configuring), static_cast<int>(app.state()));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok), static_cast<int>(app.lastConfigurationResult()));
    TEST_ASSERT_EQUAL_UINT32(0x00000001, app.deviceId());
}

TEST_CASE("deviceId zero is recorded as an invalid configuration", "[smart_sys_app]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 0}, makeHooks(scenario));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                       static_cast<int>(app.lastConfigurationResult()));
}

TEST_CASE("addSwitchPlugCapability accepts a valid config", "[smart_sys_app]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    core::SwitchPlugCapability *capability =
        app.addSwitchPlugCapability(makeSwitchConfig(1, 2));
    TEST_ASSERT_NOT_NULL(capability);
    TEST_ASSERT_FALSE(capability->state());
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                       static_cast<int>(app.lastConfigurationResult()));
}

TEST_CASE("addSwitchPlugCapability rejects an invalid GPIO", "[smart_sys_app]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    app::SwitchConfig config = makeSwitchConfig(1, 2);
    config.pin = GPIO_NUM_NC;
    core::SwitchPlugCapability *capability = app.addSwitchPlugCapability(config);
    TEST_ASSERT_NULL(capability);
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                       static_cast<int>(app.lastConfigurationResult()));
}

TEST_CASE("addSwitchPlugCapability rejects a duplicate endpoint/eventType pair", "[smart_sys_app]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    core::SwitchPlugCapability *first =
        app.addSwitchPlugCapability(makeSwitchConfig(1, 2));
    TEST_ASSERT_NOT_NULL(first);

    core::SwitchPlugCapability *duplicate =
        app.addSwitchPlugCapability(makeSwitchConfig(1, 2));
    TEST_ASSERT_NULL(duplicate);
}

TEST_CASE("addSwitchPlugCapability rejects excess capacity", "[smart_sys_app]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    for (std::uint8_t endpointId = 0; endpointId < issp::kMaxDeviceBehaviors; ++endpointId)
    {
        core::SwitchPlugCapability *capability =
            app.addSwitchPlugCapability(makeSwitchConfig(endpointId, 1));
        TEST_ASSERT_NOT_NULL(capability);
    }

    core::SwitchPlugCapability *overflow = app.addSwitchPlugCapability(
        makeSwitchConfig(static_cast<std::uint8_t>(issp::kMaxDeviceBehaviors), 1));
    TEST_ASSERT_NULL(overflow);
}

TEST_CASE("capability pointers remain stable as more capabilities are added", "[smart_sys_app]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    core::SwitchPlugCapability *first =
        app.addSwitchPlugCapability(makeSwitchConfig(1, 2));
    TEST_ASSERT_NOT_NULL(first);

    core::SwitchPlugCapability *second =
        app.addSwitchPlugCapability(makeSwitchConfig(2, 2));
    TEST_ASSERT_NOT_NULL(second);

    TEST_ASSERT_FALSE(first->state());
    TEST_ASSERT_TRUE(first != second);
}

TEST_CASE("configureFactoryResetButton accepts a valid config", "[smart_sys_app]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    const AppResult result = app.configureFactoryResetButton({
        .pin = GPIO_NUM_9,
        .activeLow = true,
        .holdTimeMs = 10000,
        .pollIntervalMs = 20,
    });
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok), static_cast<int>(result));
}

TEST_CASE("configureFactoryResetButton rejects a duplicate configuration", "[smart_sys_app]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    const app::PushButtonConfig config = {
        .pin = GPIO_NUM_9,
        .activeLow = true,
        .holdTimeMs = 10000,
        .pollIntervalMs = 20,
    };
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                       static_cast<int>(app.configureFactoryResetButton(config)));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Failed),
                       static_cast<int>(app.configureFactoryResetButton(config)));
}

TEST_CASE("configureFactoryResetButton rejects zero hold/poll times", "[smart_sys_app]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    const AppResult result = app.configureFactoryResetButton({
        .pin = GPIO_NUM_9,
        .activeLow = true,
        .holdTimeMs = 0,
        .pollIntervalMs = 20,
    });
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                       static_cast<int>(result));
}

// --- setup() state machine, order, repeated calls, failures and rollback ---

TEST_CASE("setup() with all steps succeeding reaches Running in order", "[smart_sys_app][setup]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    TEST_ASSERT_NOT_NULL(app.addSwitchPlugCapability(makeSwitchConfig(1, 2)));

    const SetupResult result = app.setup();

    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running), static_cast<int>(result.state));
    TEST_ASSERT_EQUAL(static_cast<int>(SetupStage::Completed), static_cast<int>(result.stage));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok), static_cast<int>(result.result));
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running), static_cast<int>(app.state()));

    TEST_ASSERT_EQUAL_size_t(5, scenario.callCount);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::InitializePlatform), static_cast<int>(scenario.callOrder[0]));
    TEST_ASSERT_EQUAL(static_cast<int>(Step::InitializeNetwork), static_cast<int>(scenario.callOrder[1]));
    TEST_ASSERT_EQUAL(static_cast<int>(Step::RegisterCapability), static_cast<int>(scenario.callOrder[2]));
    TEST_ASSERT_EQUAL(static_cast<int>(Step::StartDevice), static_cast<int>(scenario.callOrder[3]));
    TEST_ASSERT_EQUAL(static_cast<int>(Step::StartReportExecutor), static_cast<int>(scenario.callOrder[4]));
    TEST_ASSERT_EQUAL_size_t(0, scenario.rollbackCalls);
}

TEST_CASE("setup() registers capabilities once each, in addition order", "[smart_sys_app][setup]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    TEST_ASSERT_NOT_NULL(app.addSwitchPlugCapability(makeSwitchConfig(1, 2)));
    TEST_ASSERT_NOT_NULL(app.addSwitchPlugCapability(makeSwitchConfig(2, 2)));

    const SetupResult result = app.setup();

    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running), static_cast<int>(result.state));
    TEST_ASSERT_EQUAL_size_t(2, scenario.registerCapabilityCalls);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::RegisterCapability), static_cast<int>(scenario.callOrder[2]));
    TEST_ASSERT_EQUAL(static_cast<int>(Step::RegisterCapability), static_cast<int>(scenario.callOrder[3]));
    TEST_ASSERT_EQUAL(static_cast<int>(Step::StartDevice), static_cast<int>(scenario.callOrder[4]));
}

TEST_CASE("setup() called again after Running returns Busy without re-running hooks", "[smart_sys_app][setup]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));

    const SetupResult first = app.setup();
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running), static_cast<int>(first.state));
    const std::size_t callsAfterFirstSetup = scenario.callCount;

    const SetupResult second = app.setup();
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running), static_cast<int>(second.state));
    TEST_ASSERT_EQUAL(static_cast<int>(SetupStage::None), static_cast<int>(second.stage));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Busy), static_cast<int>(second.result));
    TEST_ASSERT_EQUAL_size_t(callsAfterFirstSetup, scenario.callCount);
}

TEST_CASE("setup() fails ValidateConfiguration without running any hook", "[smart_sys_app][setup]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 0}, makeHooks(scenario));

    const SetupResult result = app.setup();

    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Failed), static_cast<int>(result.state));
    TEST_ASSERT_EQUAL(static_cast<int>(SetupStage::ValidateConfiguration), static_cast<int>(result.stage));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument), static_cast<int>(result.result));
    TEST_ASSERT_EQUAL_size_t(0, scenario.callCount);
}

TEST_CASE("setup() fails InitializePlatform without rollback", "[smart_sys_app][setup]")
{
    FakeScenario scenario;
    scenario.initializePlatformResult = AppResult::Failed;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));

    const SetupResult result = app.setup();

    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Failed), static_cast<int>(result.state));
    TEST_ASSERT_EQUAL(static_cast<int>(SetupStage::InitializePlatform), static_cast<int>(result.stage));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Failed), static_cast<int>(result.result));
    TEST_ASSERT_EQUAL_size_t(1, scenario.callCount);
    TEST_ASSERT_EQUAL_size_t(0, scenario.rollbackCalls);
}

TEST_CASE("setup() reaches NotReady and rolls back when network is not found", "[smart_sys_app][setup]")
{
    FakeScenario scenario;
    scenario.initializeNetworkResult = AppResult::NotReady;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));

    const SetupResult result = app.setup();

    TEST_ASSERT_EQUAL(static_cast<int>(AppState::NotReady), static_cast<int>(result.state));
    TEST_ASSERT_EQUAL(static_cast<int>(SetupStage::InitializeNetwork), static_cast<int>(result.stage));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::NotReady), static_cast<int>(result.result));
    TEST_ASSERT_EQUAL_size_t(1, scenario.rollbackCalls);
}

TEST_CASE("setup() fails InitializeNetwork and rolls back on a hard failure", "[smart_sys_app][setup]")
{
    FakeScenario scenario;
    scenario.initializeNetworkResult = AppResult::Failed;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));

    const SetupResult result = app.setup();

    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Failed), static_cast<int>(result.state));
    TEST_ASSERT_EQUAL(static_cast<int>(SetupStage::InitializeNetwork), static_cast<int>(result.stage));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Failed), static_cast<int>(result.result));
    TEST_ASSERT_EQUAL_size_t(1, scenario.rollbackCalls);
}

TEST_CASE("setup() fails RegisterCapabilities, rolls back and never starts the device", "[smart_sys_app][setup]")
{
    FakeScenario scenario;
    scenario.registerCapabilityResult = AppResult::Failed;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    TEST_ASSERT_NOT_NULL(app.addSwitchPlugCapability(makeSwitchConfig(1, 2)));

    const SetupResult result = app.setup();

    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Failed), static_cast<int>(result.state));
    TEST_ASSERT_EQUAL(static_cast<int>(SetupStage::RegisterCapabilities), static_cast<int>(result.stage));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Failed), static_cast<int>(result.result));
    TEST_ASSERT_EQUAL_size_t(1, scenario.rollbackCalls);
    for (std::size_t index = 0; index < scenario.callCount; ++index)
    {
        TEST_ASSERT_NOT_EQUAL(static_cast<int>(Step::StartDevice), static_cast<int>(scenario.callOrder[index]));
        TEST_ASSERT_NOT_EQUAL(static_cast<int>(Step::StartReportExecutor), static_cast<int>(scenario.callOrder[index]));
    }
}

TEST_CASE("setup() fails StartDevice, rolls back and never starts the executor", "[smart_sys_app][setup]")
{
    FakeScenario scenario;
    scenario.startDeviceResult = AppResult::Failed;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));

    const SetupResult result = app.setup();

    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Failed), static_cast<int>(result.state));
    TEST_ASSERT_EQUAL(static_cast<int>(SetupStage::StartDevice), static_cast<int>(result.stage));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Failed), static_cast<int>(result.result));
    TEST_ASSERT_EQUAL_size_t(1, scenario.rollbackCalls);
    for (std::size_t index = 0; index < scenario.callCount; ++index)
    {
        TEST_ASSERT_NOT_EQUAL(static_cast<int>(Step::StartReportExecutor), static_cast<int>(scenario.callOrder[index]));
    }
}

TEST_CASE("setup() fails StartReportExecutor and rolls back preserving the primary error", "[smart_sys_app][setup]")
{
    FakeScenario scenario;
    scenario.startReportExecutorResult = AppResult::Failed;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));

    const SetupResult result = app.setup();

    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Failed), static_cast<int>(result.state));
    TEST_ASSERT_EQUAL(static_cast<int>(SetupStage::StartReportExecutor), static_cast<int>(result.stage));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Failed), static_cast<int>(result.result));
    TEST_ASSERT_EQUAL_size_t(1, scenario.rollbackCalls);
}

extern "C" void app_main()
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
