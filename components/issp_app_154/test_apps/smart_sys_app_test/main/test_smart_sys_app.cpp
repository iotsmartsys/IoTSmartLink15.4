// Configuration-time coverage for iotsmartsys::SmartSysApp (SMARTAPP-AC-001,
// AC-004, AC-004A, AC-005). None of these cases call setup(), so each
// SmartSysApp instance may safely use automatic storage duration per
// SMARTAPP-DEC-004A ("antes de qualquer chamada a setup(), a destruicao da
// fachada em Configuring e permitida"). setup()-dependent cases (network,
// device, executor, rollback, AC-006 Busy) require radio and NVS hardware
// and are covered by SMARTAPP-AC-022, deferred to hardware validation.

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

} // namespace

TEST_CASE("construction with a valid config starts Configuring", "[smart_sys_app]")
{
    SmartSysApp app({.deviceId = 0x00000001});
    TEST_ASSERT_EQUAL(static_cast<int>(AppState::Configuring), static_cast<int>(app.state()));
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok), static_cast<int>(app.lastConfigurationResult()));
    TEST_ASSERT_EQUAL_UINT32(0x00000001, app.deviceId());
}

TEST_CASE("deviceId zero is recorded as an invalid configuration", "[smart_sys_app]")
{
    SmartSysApp app({.deviceId = 0});
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                       static_cast<int>(app.lastConfigurationResult()));
}

TEST_CASE("addSwitchPlugCapability accepts a valid config", "[smart_sys_app]")
{
    SmartSysApp app({.deviceId = 1});
    core::SwitchPlugCapability *capability =
        app.addSwitchPlugCapability(makeSwitchConfig(1, 2));
    TEST_ASSERT_NOT_NULL(capability);
    TEST_ASSERT_FALSE(capability->state());
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::Ok),
                       static_cast<int>(app.lastConfigurationResult()));
}

TEST_CASE("addSwitchPlugCapability rejects an invalid GPIO", "[smart_sys_app]")
{
    SmartSysApp app({.deviceId = 1});
    app::SwitchConfig config = makeSwitchConfig(1, 2);
    config.pin = GPIO_NUM_NC;
    core::SwitchPlugCapability *capability = app.addSwitchPlugCapability(config);
    TEST_ASSERT_NULL(capability);
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                       static_cast<int>(app.lastConfigurationResult()));
}

TEST_CASE("addSwitchPlugCapability rejects a duplicate endpoint/eventType pair", "[smart_sys_app]")
{
    SmartSysApp app({.deviceId = 1});
    core::SwitchPlugCapability *first =
        app.addSwitchPlugCapability(makeSwitchConfig(1, 2));
    TEST_ASSERT_NOT_NULL(first);

    core::SwitchPlugCapability *duplicate =
        app.addSwitchPlugCapability(makeSwitchConfig(1, 2));
    TEST_ASSERT_NULL(duplicate);
}

TEST_CASE("addSwitchPlugCapability rejects excess capacity", "[smart_sys_app]")
{
    SmartSysApp app({.deviceId = 1});
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
    SmartSysApp app({.deviceId = 1});
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
    SmartSysApp app({.deviceId = 1});
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
    SmartSysApp app({.deviceId = 1});
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
    SmartSysApp app({.deviceId = 1});
    const AppResult result = app.configureFactoryResetButton({
        .pin = GPIO_NUM_9,
        .activeLow = true,
        .holdTimeMs = 0,
        .pollIntervalMs = 20,
    });
    TEST_ASSERT_EQUAL(static_cast<int>(AppResult::InvalidArgument),
                       static_cast<int>(result));
}

extern "C" void app_main()
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
