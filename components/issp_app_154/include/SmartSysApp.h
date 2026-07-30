#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "driver/gpio.h"

#include "digital_output_behavior.hpp"
#include "issp154_network_manager.hpp"
#include "issp154_report_executor.hpp"
#include "issp154_transport.hpp"
#include "issp_device.hpp"
#include "issp_limits.hpp"

#include "reset/factory_reset_service.hpp"
#include "reset/reset_button_monitor.hpp"

namespace iotsmartsys::app
{

struct SmartSysAppConfig
{
    std::uint32_t deviceId;
};

struct SwitchConfig
{
    gpio_num_t pin;
    bool activeHigh;
    bool initialState;
    bool reportOnStart;
    std::uint8_t endpointId;
    std::uint8_t eventType;
};

struct PushButtonConfig
{
    gpio_num_t pin;
    bool activeLow;
    std::uint32_t holdTimeMs;
    std::uint32_t pollIntervalMs;
};

} // namespace iotsmartsys::app

namespace iotsmartsys
{
class SmartSysApp;
}

namespace iotsmartsys::core
{

class SwitchPlugCapability
{
public:
    explicit SwitchPlugCapability(issp::DigitalOutputBehavior &behavior);

    bool state() const;

private:
    issp::DigitalOutputBehavior *behavior_;
};

} // namespace iotsmartsys::core

namespace iotsmartsys
{

enum class AppState : std::uint8_t
{
    Configuring,
    Starting,
    Running,
    NotReady,
    Failed
};

enum class SetupStage : std::uint8_t
{
    None,
    InitializePlatform,
    ValidateConfiguration,
    InitializeNetwork,
    RegisterCapabilities,
    StartDevice,
    StartReportExecutor,
    Completed
};

enum class AppResult : std::uint8_t
{
    Ok,
    InvalidArgument,
    NotReady,
    Busy,
    Failed
};

struct SetupResult
{
    AppState state;
    SetupStage stage;
    AppResult result;
};

class SmartSysApp
{
public:
    explicit SmartSysApp(const app::SmartSysAppConfig &config);

    core::SwitchPlugCapability *
    addSwitchPlugCapability(const app::SwitchConfig &config);

    AppResult
    configureFactoryResetButton(const app::PushButtonConfig &config);

    SetupResult setup();

    AppState state() const;
    SetupResult lastSetupResult() const;
    AppResult lastConfigurationResult() const;
    std::uint32_t deviceId() const;

private:
    static constexpr std::size_t kMaxSwitchCapabilities =
        issp::kMaxDeviceBehaviors;
    static constexpr std::uint16_t kShortAddress = 0x1001;

    static esp_err_t clearNetworkConfiguration(void *context);

    void recordConfigurationFailure(AppResult result);
    bool hasDuplicateSwitchEndpoint(const app::SwitchConfig &config) const;

    SetupResult fail(SetupStage stage, AppResult result);
    void shutdownTransportAfterFailure();

    app::SmartSysAppConfig config_;
    AppState state_;
    SetupResult lastSetupResult_;
    AppResult lastConfigurationResult_;

    std::array<app::SwitchConfig, kMaxSwitchCapabilities> switchConfigs_;
    std::array<std::optional<issp::DigitalOutputBehavior>, kMaxSwitchCapabilities>
        switchBehaviors_;
    std::array<std::optional<core::SwitchPlugCapability>, kMaxSwitchCapabilities>
        switchCapabilities_;
    std::size_t switchCount_;

    bool factoryResetConfigured_;
    app::PushButtonConfig factoryResetConfig_;

    std::array<std::uint8_t, issp::kIssp154ExtendedAddressSize> extendedAddress_;
    std::optional<issp::Issp154Transport> transport_;
    std::optional<issp::Issp154NetworkManager> networkManager_;
    std::optional<issp::IsspDevice> device_;
    std::optional<issp::Issp154ReportExecutor> reportExecutor_;
    std::optional<FactoryResetService> factoryResetService_;
    std::optional<ResetButtonMonitor> resetButtonMonitor_;
};

} // namespace iotsmartsys
