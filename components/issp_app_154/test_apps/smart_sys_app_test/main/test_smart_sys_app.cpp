// Automated coverage for iotsmartsys::SmartSysApp: configuration validation
// (SMARTAPP-AC-001, AC-004, AC-004A, AC-005), states, initialization order,
// repeated setup(), injected failures and rollback (SMARTAPP-AC-006 to
// AC-013), and the deep sleep lifecycle (DEEPSLEEP-AC-001 to AC-003, AC-006 to
// AC-008, and the part of AC-011 that doubles can observe), driven through the
// deep-sleep seam of SetupHooks so that no wakeup source is armed, no LED GPIO
// is touched and no case ever sleeps for real.
// Every SmartSysApp instance here is built with
// SmartSysApp::SetupHooks, replacing the platform/network/device/executor
// steps with fakes, so nothing in this file ever calls NVS, GPIO drivers or
// radio APIs. The app targets a physical esp32h2, the target bound to
// client_154 and SmartSysApp (TESTEXEC-008). smart_sys_app_hardware.cpp and
// the production single-argument constructor are linked in on this target but
// no case here reaches them, so the runner still observes only the
// fake-backed state machine defined by these tests.

#include <array>
#include <atomic>
#include <cstddef>

#include "SmartSysApp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

app::DoorSensorConfig makeDoorSensorConfig(std::uint8_t endpointId,
                                           std::uint8_t eventType)
{
    return {
        .pin = GPIO_NUM_14,
        .activeHigh = true,
        .pull = app::DigitalInputPull::PullUp,
        .reportOnStart = true,
        .endpointId = endpointId,
        .eventType = eventType,
        .samplePeriodMs = 10,
        .samplesPerWindow = 5,
        .majorityThreshold = 3,
        .consecutiveWindows = 2,
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
    PrepareTimerWakeup,
    PrepareContactWakeup,
    StopResetButtonMonitor,
    BeginDeviceQuiescence,
    StopReportExecutor,
    EndTransport,
    EnterDeepSleep,
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

    // Deep-sleep seam: lets each test script the wakeup source, the delivery
    // oracle and the bounded stop, and observe the terminal sequence without
    // ever entering a real deep sleep.
    AppResult prepareTimerWakeupResult = AppResult::Ok;
    AppResult prepareContactWakeupResult = AppResult::Ok;
    gpio_num_t preparedContactPin = GPIO_NUM_NC;
    app::DigitalInputPull preparedContactPull = app::DigitalInputPull::Floating;
    AppResult beginDeviceQuiescenceResult = AppResult::Ok;
    AppResult stopReportExecutorResult = AppResult::Ok;
    std::uint32_t stopReportExecutorDelayMs = 0;
    std::size_t pendingReportCountValue = 0;
    std::uint64_t preparedSleepUs = 0;
    std::size_t endTransportCalls = 0;
    std::size_t stopResetButtonMonitorCalls = 0;
    std::atomic<bool> deepSleepEntered{false};

    static constexpr std::size_t kMaxCalls = 24;
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

    bool recorded(Step step) const
    {
        for (std::size_t index = 0; index < callCount && index < kMaxCalls; ++index)
        {
            if (callOrder[index] == step)
            {
                return true;
            }
        }
        return false;
    }

    std::size_t indexOf(Step step) const
    {
        for (std::size_t index = 0; index < callCount && index < kMaxCalls; ++index)
        {
            if (callOrder[index] == step)
            {
                return index;
            }
        }
        return kMaxCalls;
    }
};

// The lifecycle sequence runs in its own task, so every deep-sleep case waits
// for the observable outcome instead of assuming a scheduling order.
bool waitForDeepSleep(const FakeScenario &scenario, std::uint32_t timeoutMs)
{
    const std::int64_t deadlineUs =
        esp_timer_get_time() + static_cast<std::int64_t>(timeoutMs) * 1000;
    while (!scenario.deepSleepEntered.load())
    {
        if (esp_timer_get_time() >= deadlineUs)
        {
            return false;
        }
        vTaskDelay(1);
    }
    // Lets the lifecycle task reach vTaskDelete() before the SmartSysApp that
    // owns its static stack leaves scope.
    vTaskDelay(pdMS_TO_TICKS(50));
    return true;
}

app::DeepSleepConfig makeDeepSleepConfig(std::uint32_t maxAwakeTimeMs)
{
    return {
        .enabled = true,
        .maxAwakeTimeMs = maxAwakeTimeMs,
        .timerWakeup = {
            .enabled = true,
            .interval = 15,
            .unit = app::DeepSleepTimeUnit::Minutes,
        },
        // Disabled in every case that reaches setup(): lighting it would touch
        // a GPIO driver, which this app deliberately never does.
        .wakeLed = {
            .enabled = false,
            .pin = GPIO_NUM_13,
            .activeHigh = true,
            .onMode = app::WakeLedOnMode::DurationMs,
            .onTimeMs = 200,
        },
        // Disabled by default: the cases that exercise the contact enable it
        // explicitly, and the real pad/EXT1 preparation is replaced by the seam
        // so that no GPIO is touched and no wakeup source is armed.
        .contactWakeup = {
            .enabled = false,
            .pin = GPIO_NUM_14,
        },
    };
}

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

AppResult fakeBeginDeviceQuiescence(void *context)
{
    auto *scenario = static_cast<FakeScenario *>(context);
    scenario->record(Step::BeginDeviceQuiescence);
    return scenario->beginDeviceQuiescenceResult;
}

AppResult fakeStopReportExecutor(void *context)
{
    auto *scenario = static_cast<FakeScenario *>(context);
    scenario->record(Step::StopReportExecutor);
    if (scenario->stopReportExecutorDelayMs != 0)
    {
        vTaskDelay(pdMS_TO_TICKS(scenario->stopReportExecutorDelayMs));
    }
    return scenario->stopReportExecutorResult;
}

void fakeEndTransport(void *context)
{
    auto *scenario = static_cast<FakeScenario *>(context);
    scenario->record(Step::EndTransport);
    ++scenario->endTransportCalls;
}

std::size_t fakePendingReportCount(void *context)
{
    return static_cast<FakeScenario *>(context)->pendingReportCountValue;
}

void fakeStopResetButtonMonitor(void *context)
{
    auto *scenario = static_cast<FakeScenario *>(context);
    scenario->record(Step::StopResetButtonMonitor);
    ++scenario->stopResetButtonMonitorCalls;
}

AppResult fakePrepareTimerWakeup(void *context, std::uint64_t sleepUs)
{
    auto *scenario = static_cast<FakeScenario *>(context);
    scenario->record(Step::PrepareTimerWakeup);
    scenario->preparedSleepUs = sleepUs;
    return scenario->prepareTimerWakeupResult;
}

AppResult fakePrepareContactWakeup(void *context, gpio_num_t pin,
                                   app::DigitalInputPull pull)
{
    auto *scenario = static_cast<FakeScenario *>(context);
    scenario->record(Step::PrepareContactWakeup);
    scenario->preparedContactPin = pin;
    scenario->preparedContactPull = pull;
    return scenario->prepareContactWakeupResult;
}

void fakeEnterDeepSleep(void *context)
{
    auto *scenario = static_cast<FakeScenario *>(context);
    scenario->record(Step::EnterDeepSleep);
    scenario->deepSleepEntered.store(true);
}

SmartSysApp::SetupHooks makeHooks(FakeScenario &scenario)
{
    return SmartSysApp::SetupHooks{
        .initializePlatform = &fakeInitializePlatform,
        .initializeNetwork = &fakeInitializeNetwork,
        .registerCapability = &fakeRegisterCapability,
        .startDevice = &fakeStartDevice,
        .startReportExecutor = &fakeStartReportExecutor,
        .rollbackTransport = &fakeRollbackTransport,
        .context = &scenario,
        .beginDeviceQuiescence = &fakeBeginDeviceQuiescence,
        .stopReportExecutor = &fakeStopReportExecutor,
        .endTransport = &fakeEndTransport,
        .pendingReportCount = &fakePendingReportCount,
        .stopResetButtonMonitor = &fakeStopResetButtonMonitor,
        .prepareTimerWakeup = &fakePrepareTimerWakeup,
        .prepareContactWakeup = &fakePrepareContactWakeup,
        .enterDeepSleep = &fakeEnterDeepSleep,
        // A limit small enough to be crossed by a 32-bit interval in hours,
        // without depending on the value the target derives.
        .maxTimerWakeupUs = 3600ULL * 1000000ULL * 24ULL,
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

TEST_CASE("addDoorSensorCapability accepts a valid config", "[smart_sys_app][door]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    core::DoorSensorCapability *capability =
        app.addDoorSensorCapability(makeDoorSensorConfig(1, 1));
    TEST_ASSERT_NOT_NULL(capability);
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(app.lastConfigurationResult()));
}

TEST_CASE("addDoorSensorCapability rejects an invalid pin", "[smart_sys_app][door]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    app::DoorSensorConfig config = makeDoorSensorConfig(1, 1);
    config.pin = GPIO_NUM_NC;
    TEST_ASSERT_NULL(app.addDoorSensorCapability(config));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                      static_cast<int>(app.lastConfigurationResult()));
}

TEST_CASE("addDoorSensorCapability rejects an invalid debounce configuration",
          "[smart_sys_app][door]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    app::DoorSensorConfig config = makeDoorSensorConfig(1, 1);
    config.majorityThreshold = 6;
    TEST_ASSERT_NULL(app.addDoorSensorCapability(config));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                      static_cast<int>(app.lastConfigurationResult()));
}

TEST_CASE("endpoint and event pairs are unique across capability types",
          "[smart_sys_app][door]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    TEST_ASSERT_NOT_NULL(app.addSwitchPlugCapability(makeSwitchConfig(1, 1)));
    TEST_ASSERT_NULL(app.addDoorSensorCapability(makeDoorSensorConfig(1, 1)));
}

// The hook only reports an index, so it cannot distinguish which capability
// type was registered first. This case proves that both entries of the unified
// registry are registered; the order by type remains static evidence of the
// unified vector, without adding a seam to SetupHooks.
TEST_CASE("setup registers both capabilities of the unified registry",
          "[smart_sys_app][door][setup]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    TEST_ASSERT_NOT_NULL(app.addSwitchPlugCapability(makeSwitchConfig(1, 2)));
    TEST_ASSERT_NOT_NULL(app.addDoorSensorCapability(makeDoorSensorConfig(1, 1)));

    const SetupResult result = app.setup();
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running), static_cast<int>(result.state));
    TEST_ASSERT_EQUAL_size_t(2, scenario.registerCapabilityCalls);
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

// --- deep sleep (docs/specs/Client-Deep-Sleep.md) ---
//
// Every case below drives the lifecycle through the deep-sleep seam of
// SetupHooks, so no wakeup source is armed, no GPIO is touched and the device
// never actually sleeps.

TEST_CASE("without opt-in nothing of deep sleep is started",
          "[smart_sys_app][deep_sleep]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));

    const SetupResult result = app.setup();

    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running), static_cast<int>(result.state));
    TEST_ASSERT_FALSE(waitForDeepSleep(scenario, 300));
    TEST_ASSERT_FALSE(scenario.recorded(Step::PrepareTimerWakeup));
    TEST_ASSERT_FALSE(scenario.recorded(Step::BeginDeviceQuiescence));
}

TEST_CASE("deep sleep disabled preserves the current runtime",
          "[smart_sys_app][deep_sleep]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    app::DeepSleepConfig config = makeDeepSleepConfig(50);
    config.enabled = false;

    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(app.configureDeepSleep(config)));
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running),
                      static_cast<int>(app.setup().state));
    TEST_ASSERT_FALSE(waitForDeepSleep(scenario, 300));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(app.lastConfigurationResult()));
}

TEST_CASE("configureDeepSleep rejects a zero maximum awake time",
          "[smart_sys_app][deep_sleep]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    app::DeepSleepConfig config = makeDeepSleepConfig(0);

    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                      static_cast<int>(app.configureDeepSleep(config)));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                      static_cast<int>(app.lastConfigurationResult()));
}

TEST_CASE("configureDeepSleep rejects a duplicate call",
          "[smart_sys_app][deep_sleep]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(app.configureDeepSleep(makeDeepSleepConfig(1000))));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Failed),
                      static_cast<int>(app.configureDeepSleep(makeDeepSleepConfig(1000))));
}

TEST_CASE("configureDeepSleep rejects a late call", "[smart_sys_app][deep_sleep]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running),
                      static_cast<int>(app.setup().state));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Failed),
                      static_cast<int>(app.configureDeepSleep(makeDeepSleepConfig(1000))));
}

TEST_CASE("configureDeepSleep rejects an invalid timer", "[smart_sys_app][deep_sleep]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    app::DeepSleepConfig config = makeDeepSleepConfig(1000);
    config.timerWakeup.interval = 0;
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                      static_cast<int>(app.configureDeepSleep(config)));
}

TEST_CASE("configureDeepSleep rejects an interval above the accepted limit",
          "[smart_sys_app][deep_sleep]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    app::DeepSleepConfig config = makeDeepSleepConfig(1000);
    // The seam limit is 24 hours, so 25 hours must be refused while 24 is not.
    config.timerWakeup.interval = 25;
    config.timerWakeup.unit = app::DeepSleepTimeUnit::Hours;
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                      static_cast<int>(app.configureDeepSleep(config)));

    FakeScenario acceptedScenario;
    SmartSysApp accepted({.deviceId = 1}, makeHooks(acceptedScenario));
    config.timerWakeup.interval = 24;
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(accepted.configureDeepSleep(config)));
}

TEST_CASE("configureDeepSleep rejects a wake LED without a duration",
          "[smart_sys_app][deep_sleep]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    app::DeepSleepConfig config = makeDeepSleepConfig(1000);
    config.wakeLed.enabled = true;
    config.wakeLed.onTimeMs = 0;
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                      static_cast<int>(app.configureDeepSleep(config)));
}

TEST_CASE("the wake LED GPIO cannot collide with a capability, in either order",
          "[smart_sys_app][deep_sleep]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    app::SwitchConfig switchConfig = makeSwitchConfig(1, 2);
    switchConfig.pin = GPIO_NUM_13;
    TEST_ASSERT_NOT_NULL(app.addSwitchPlugCapability(switchConfig));

    app::DeepSleepConfig config = makeDeepSleepConfig(1000);
    config.wakeLed.enabled = true;
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                      static_cast<int>(app.configureDeepSleep(config)));

    FakeScenario inverseScenario;
    SmartSysApp inverse({.deviceId = 1}, makeHooks(inverseScenario));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(inverse.configureDeepSleep(config)));
    TEST_ASSERT_NULL(inverse.addSwitchPlugCapability(switchConfig));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                      static_cast<int>(inverse.lastConfigurationResult()));
}

TEST_CASE("the deadline drives the forced path in the mandatory order",
          "[smart_sys_app][deep_sleep]")
{
    FakeScenario scenario;
    // A pending report that is never delivered: only the deadline may sleep.
    scenario.pendingReportCountValue = 1;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(app.configureDeepSleep(makeDeepSleepConfig(200))));
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running),
                      static_cast<int>(app.setup().state));

    TEST_ASSERT_TRUE(waitForDeepSleep(scenario, 3000));
    TEST_ASSERT_TRUE(scenario.indexOf(Step::PrepareTimerWakeup) <
                     scenario.indexOf(Step::StopResetButtonMonitor));
    TEST_ASSERT_TRUE(scenario.indexOf(Step::StopResetButtonMonitor) <
                     scenario.indexOf(Step::BeginDeviceQuiescence));
    TEST_ASSERT_TRUE(scenario.indexOf(Step::BeginDeviceQuiescence) <
                     scenario.indexOf(Step::StopReportExecutor));
    TEST_ASSERT_TRUE(scenario.indexOf(Step::StopReportExecutor) <
                     scenario.indexOf(Step::EndTransport));
    TEST_ASSERT_TRUE(scenario.indexOf(Step::EndTransport) <
                     scenario.indexOf(Step::EnterDeepSleep));
}

TEST_CASE("minutes and hours convert without semantic loss",
          "[smart_sys_app][deep_sleep]")
{
    FakeScenario minutesScenario;
    SmartSysApp minutesApp({.deviceId = 1}, makeHooks(minutesScenario));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(minutesApp.configureDeepSleep(makeDeepSleepConfig(50))));
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running),
                      static_cast<int>(minutesApp.setup().state));
    TEST_ASSERT_TRUE(waitForDeepSleep(minutesScenario, 3000));
    TEST_ASSERT_TRUE(minutesScenario.preparedSleepUs == 15ULL * 60ULL * 1000000ULL);

    FakeScenario hoursScenario;
    SmartSysApp hoursApp({.deviceId = 1}, makeHooks(hoursScenario));
    app::DeepSleepConfig hours = makeDeepSleepConfig(50);
    hours.timerWakeup.interval = 2;
    hours.timerWakeup.unit = app::DeepSleepTimeUnit::Hours;
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(hoursApp.configureDeepSleep(hours)));
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running),
                      static_cast<int>(hoursApp.setup().state));
    TEST_ASSERT_TRUE(waitForDeepSleep(hoursScenario, 3000));
    TEST_ASSERT_TRUE(hoursScenario.preparedSleepUs == 2ULL * 3600ULL * 1000000ULL);
}

TEST_CASE("a failed wakeup source blocks the sleep and keeps the runtime reachable",
          "[smart_sys_app][deep_sleep]")
{
    FakeScenario scenario;
    scenario.prepareTimerWakeupResult = AppResult::InvalidArgument;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(app.configureDeepSleep(makeDeepSleepConfig(100))));
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running),
                      static_cast<int>(app.setup().state));

    TEST_ASSERT_FALSE(waitForDeepSleep(scenario, 1000));
    TEST_ASSERT_TRUE(scenario.recorded(Step::PrepareTimerWakeup));
    TEST_ASSERT_FALSE(scenario.recorded(Step::StopResetButtonMonitor));
    TEST_ASSERT_FALSE(scenario.recorded(Step::BeginDeviceQuiescence));
    TEST_ASSERT_FALSE(scenario.recorded(Step::StopReportExecutor));
}

TEST_CASE("an expired stop budget suppresses the transport shutdown",
          "[smart_sys_app][deep_sleep]")
{
    FakeScenario scenario;
    scenario.stopReportExecutorDelayMs = 700;
    scenario.stopReportExecutorResult = AppResult::Busy;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(app.configureDeepSleep(makeDeepSleepConfig(100))));
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running),
                      static_cast<int>(app.setup().state));

    TEST_ASSERT_TRUE(waitForDeepSleep(scenario, 5000));
    TEST_ASSERT_EQUAL_size_t(0, scenario.endTransportCalls);
}

TEST_CASE("early sleep requires an expected initial report",
          "[smart_sys_app][deep_sleep]")
{
    // No capability declares reportOnStart, so there is no positive evidence and
    // only the deadline may authorize the sleep.
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    TEST_ASSERT_NOT_NULL(app.addSwitchPlugCapability(makeSwitchConfig(1, 2)));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(app.configureDeepSleep(makeDeepSleepConfig(400))));

    const std::int64_t startUs = esp_timer_get_time();
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running),
                      static_cast<int>(app.setup().state));
    TEST_ASSERT_TRUE(waitForDeepSleep(scenario, 3000));
    const std::int64_t elapsedMs = (esp_timer_get_time() - startUs) / 1000;
    TEST_ASSERT_TRUE(elapsedMs >= 400);
}

TEST_CASE("an admitted initial report with nothing pending sleeps early",
          "[smart_sys_app][deep_sleep]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    app::SwitchConfig switchConfig = makeSwitchConfig(1, 2);
    // DigitalOutputBehavior publishes synchronously in begin(), so reaching
    // Running is itself the evidence that the initial report was admitted.
    switchConfig.reportOnStart = true;
    TEST_ASSERT_NOT_NULL(app.addSwitchPlugCapability(switchConfig));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(app.configureDeepSleep(makeDeepSleepConfig(10000))));

    const std::int64_t startUs = esp_timer_get_time();
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running),
                      static_cast<int>(app.setup().state));
    TEST_ASSERT_TRUE(waitForDeepSleep(scenario, 3000));
    const std::int64_t elapsedMs = (esp_timer_get_time() - startUs) / 1000;
    TEST_ASSERT_TRUE(elapsedMs < 10000);
}

// --- dry contact wakeup (DEEPSLEEP-AC-011) ---
//
// The electrical rearming itself -- reapplying the pad, reading its level and
// arming EXT1 for the opposite one -- is only satisfied by hardware evidence
// (DEEPSLEEP-AC-012). These cases cover what doubles can observe: the
// eligibility of the GPIO, the correspondence with a registered capability, the
// position of the preparation in the terminal sequence and the block on failure.

TEST_CASE("configureDeepSleep rejects a contact GPIO the target cannot wake on",
          "[smart_sys_app][deep_sleep][contact]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    app::DeepSleepConfig config = makeDeepSleepConfig(1000);
    config.contactWakeup.enabled = true;
    // A valid GPIO outside the range the ESP32-H2 accepts as an external wakeup
    // source; the facade derives eligibility from the target, not from a list
    // of its own.
    config.contactWakeup.pin = GPIO_NUM_2;

    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                      static_cast<int>(app.configureDeepSleep(config)));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                      static_cast<int>(app.lastConfigurationResult()));
}

TEST_CASE("a contact wakeup without a matching capability fails ValidateConfiguration",
          "[smart_sys_app][deep_sleep][contact]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    app::DeepSleepConfig config = makeDeepSleepConfig(1000);
    config.contactWakeup.enabled = true;
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(app.configureDeepSleep(config)));

    const SetupResult result = app.setup();

    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Failed), static_cast<int>(result.state));
    TEST_ASSERT_EQUAL(static_cast<int>(SetupStage::ValidateConfiguration),
                      static_cast<int>(result.stage));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                      static_cast<int>(result.result));
    TEST_ASSERT_EQUAL_size_t(0, scenario.callCount);
}

TEST_CASE("capabilities sharing the contact GPIO with divergent pulls are rejected",
          "[smart_sys_app][deep_sleep][contact]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    TEST_ASSERT_NOT_NULL(app.addDoorSensorCapability(makeDoorSensorConfig(1, 1)));
    app::DoorSensorConfig divergent = makeDoorSensorConfig(2, 1);
    divergent.pull = app::DigitalInputPull::PullDown;
    TEST_ASSERT_NOT_NULL(app.addDoorSensorCapability(divergent));

    app::DeepSleepConfig config = makeDeepSleepConfig(1000);
    config.contactWakeup.enabled = true;
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(app.configureDeepSleep(config)));

    const SetupResult result = app.setup();

    TEST_ASSERT_EQUAL(static_cast<int>(SetupStage::ValidateConfiguration),
                      static_cast<int>(result.stage));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                      static_cast<int>(result.result));
    TEST_ASSERT_EQUAL_size_t(0, scenario.callCount);

    // The same GPIO with equal pulls is electrically equivalent and accepted.
    FakeScenario equalScenario;
    SmartSysApp equal({.deviceId = 1}, makeHooks(equalScenario));
    TEST_ASSERT_NOT_NULL(equal.addDoorSensorCapability(makeDoorSensorConfig(1, 1)));
    TEST_ASSERT_NOT_NULL(equal.addDoorSensorCapability(makeDoorSensorConfig(2, 1)));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(equal.configureDeepSleep(config)));
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running),
                      static_cast<int>(equal.setup().state));
    TEST_ASSERT_TRUE(waitForDeepSleep(equalScenario, 3000));
}

TEST_CASE("both sources are armed before any terminal operation, in either order",
          "[smart_sys_app][deep_sleep][contact]")
{
    FakeScenario scenario;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    app::DeepSleepConfig config = makeDeepSleepConfig(200);
    config.contactWakeup.enabled = true;
    // configureDeepSleep() before the capability: the correspondence is only
    // checked at ValidateConfiguration, so this order is as valid as the other.
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(app.configureDeepSleep(config)));
    TEST_ASSERT_NOT_NULL(app.addDoorSensorCapability(makeDoorSensorConfig(1, 1)));
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running),
                      static_cast<int>(app.setup().state));

    TEST_ASSERT_TRUE(waitForDeepSleep(scenario, 3000));
    TEST_ASSERT_TRUE(scenario.indexOf(Step::PrepareTimerWakeup) <
                     scenario.indexOf(Step::PrepareContactWakeup));
    TEST_ASSERT_TRUE(scenario.indexOf(Step::PrepareContactWakeup) <
                     scenario.indexOf(Step::StopResetButtonMonitor));
    // The pad is reapplied from the configuration of the matching capability.
    TEST_ASSERT_EQUAL(static_cast<int>(GPIO_NUM_14),
                      static_cast<int>(scenario.preparedContactPin));
    TEST_ASSERT_EQUAL(static_cast<int>(app::DigitalInputPull::PullUp),
                      static_cast<int>(scenario.preparedContactPull));
}

TEST_CASE("a failed contact preparation blocks the sleep even with the timer armed",
          "[smart_sys_app][deep_sleep][contact]")
{
    FakeScenario scenario;
    scenario.prepareContactWakeupResult = AppResult::Failed;
    SmartSysApp app({.deviceId = 1}, makeHooks(scenario));
    app::DeepSleepConfig config = makeDeepSleepConfig(100);
    config.contactWakeup.enabled = true;
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                      static_cast<int>(app.configureDeepSleep(config)));
    TEST_ASSERT_NOT_NULL(app.addDoorSensorCapability(makeDoorSensorConfig(1, 1)));
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Running),
                      static_cast<int>(app.setup().state));

    TEST_ASSERT_FALSE(waitForDeepSleep(scenario, 1000));
    TEST_ASSERT_TRUE(scenario.recorded(Step::PrepareTimerWakeup));
    TEST_ASSERT_TRUE(scenario.recorded(Step::PrepareContactWakeup));
    TEST_ASSERT_FALSE(scenario.recorded(Step::StopResetButtonMonitor));
    TEST_ASSERT_FALSE(scenario.recorded(Step::BeginDeviceQuiescence));
    TEST_ASSERT_FALSE(scenario.recorded(Step::StopReportExecutor));
}

extern "C" void app_main()
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
