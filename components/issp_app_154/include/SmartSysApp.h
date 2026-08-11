#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"

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

enum class DigitalInputPull : std::uint8_t
{
    Floating,
    PullUp,
    PullDown,
};

struct DoorSensorConfig
{
    gpio_num_t pin;
    bool activeHigh;
    DigitalInputPull pull;
    bool reportOnStart;
    std::uint8_t endpointId;
    std::uint8_t eventType;
    std::uint32_t samplePeriodMs;
    std::uint8_t samplesPerWindow;
    std::uint8_t majorityThreshold;
    std::uint8_t consecutiveWindows;
};

struct PushButtonConfig
{
    gpio_num_t pin;
    bool activeLow;
    std::uint32_t holdTimeMs;
    std::uint32_t pollIntervalMs;
};

} // namespace iotsmartsys::app

namespace iotsmartsys::core
{

// A capability never exposes the behavior it wraps: state() is served
// through an opaque function-pointer/context pair, the same callback shape
// already used internally by the ISSP runtime (e.g. IsspDevice::CommandHandler),
// so this header never needs to name a protocol or transport type.
class SwitchPlugCapability
{
public:
    using StateFn = bool (*)(void *context);

    SwitchPlugCapability(StateFn stateFn, void *context);

    bool state() const;

private:
    StateFn stateFn_;
    void *context_;
};

class DoorSensorCapability
{
public:
    using StateFn = bool (*)(void *context);

    DoorSensorCapability(StateFn stateFn, void *context);
    bool state() const;

private:
    StateFn stateFn_;
    void *context_;
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
    // Seam that lets automated tests substitute the hardware/NVS/radio steps
    // of setup() with fakes, so the state machine, initialization order,
    // repeated-setup() Busy handling, injected failures and rollback can be
    // exercised without touching hardware. It is not part of the normative
    // product contract (specification section 8): production firmware must
    // use the single-argument constructor below, which wires the real
    // platform/network/device/executor steps internally.
    struct SetupHooks
    {
        AppResult (*initializePlatform)(void *context);
        AppResult (*initializeNetwork)(void *context);
        AppResult (*registerCapability)(void *context, std::size_t index);
        AppResult (*startDevice)(void *context);
        AppResult (*startReportExecutor)(void *context);
        void (*rollbackTransport)(void *context);
        void *context;
    };

    explicit SmartSysApp(const app::SmartSysAppConfig &config);
    SmartSysApp(const app::SmartSysAppConfig &config, const SetupHooks &hooks);
    ~SmartSysApp();

    SmartSysApp(const SmartSysApp &) = delete;
    SmartSysApp &operator=(const SmartSysApp &) = delete;
    SmartSysApp(SmartSysApp &&) = delete;
    SmartSysApp &operator=(SmartSysApp &&) = delete;

    core::SwitchPlugCapability *
    addSwitchPlugCapability(const app::SwitchConfig &config);

    core::DoorSensorCapability *
    addDoorSensorCapability(const app::DoorSensorConfig &config);

    AppResult
    configureFactoryResetButton(const app::PushButtonConfig &config);

    SetupResult setup();

    AppState state() const;
    SetupResult lastSetupResult() const;
    AppResult lastConfigurationResult() const;
    std::uint32_t deviceId() const;

public:
    // Forward declaration only -- no member is visible here. It is public
    // so smart_sys_app.cpp and smart_sys_app_hardware.cpp (the only two
    // translation units that define Impl and its members) can name
    // SmartSysApp::Impl from free functions in those files; it leaks no
    // protocol, transport or reset detail into this header.
    struct Impl;

    // Sized generously and verified by a static_assert against the real
    // Impl definition in smart_sys_app.cpp. Public only because it sizes a
    // private member; it names no protocol or transport type.
    static constexpr std::size_t kImplStorageBytes = 10240;

private:
    Impl &impl();
    const Impl &impl() const;

    alignas(alignof(std::max_align_t)) unsigned char implStorage_[kImplStorageBytes];
};

} // namespace iotsmartsys
