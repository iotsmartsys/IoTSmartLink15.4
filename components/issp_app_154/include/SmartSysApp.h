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

enum class DeepSleepTimeUnit : std::uint8_t
{
    Minutes,
    Hours,
};

enum class WakeLedOnMode : std::uint8_t
{
    DurationMs,
    UntilSleep,
};

struct TimerWakeupConfig
{
    bool enabled;
    std::uint32_t interval;
    DeepSleepTimeUnit unit;
};

struct WakeLedConfig
{
    bool enabled;
    gpio_num_t pin;
    bool activeHigh;
    WakeLedOnMode onMode;
    std::uint32_t onTimeMs;
};

// The product hands over the GPIO it received from the board model; the facade
// never discovers pinout on its own. It carries no logical polarity: the
// rearming is electrical -- the level read at the start of the terminal
// sequence decides the opposite level armed on EXT1 -- so a polarity field
// would have no effect.
struct ContactWakeupConfig
{
    bool enabled;
    gpio_num_t pin;
};

struct DeepSleepConfig
{
    bool enabled;
    std::uint32_t maxAwakeTimeMs;
    TimerWakeupConfig timerWakeup;
    WakeLedConfig wakeLed;
    // Appended last, so a composition that does not declare it stays valid and
    // the field remains inert.
    ContactWakeupConfig contactWakeup;
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

        // Deep-sleep seam (specification section 7, DEEPSLEEP-AC-010). It lets
        // the lifecycle, the validation and the failures be verified with
        // doubles; like the steps above it is not part of the normative product
        // contract. Every entry left null keeps the target-agnostic default:
        // the runtime steps behind hardwareStorage_ are skipped as a successful
        // no-op -- which is also the contract when the corresponding object was
        // never started -- while the wakeup source and the sleep entry fall back
        // to the real ESP-IDF calls.
        AppResult (*beginDeviceQuiescence)(void *context);
        // Ok when the executor task terminated inside its bounded wait, Busy
        // when that wait expired; Busy suppresses the transport shutdown.
        AppResult (*stopReportExecutor)(void *context);
        void (*endTransport)(void *context);
        std::size_t (*pendingReportCount)(void *context);
        void (*stopResetButtonMonitor)(void *context);
        AppResult (*prepareTimerWakeup)(void *context, std::uint64_t sleepUs);
        // Prepares the dry-contact source: reapplies to the pad the input mode
        // and the pull of the matching capability, reads the electrical level
        // and arms the external wakeup for the opposite one. Null falls back to
        // the real ESP-IDF calls.
        AppResult (*prepareContactWakeup)(void *context, gpio_num_t pin,
                                          app::DigitalInputPull pull);
        void (*enterDeepSleep)(void *context);
        // Zero derives the limit from the target capabilities and from the slow
        // clock source fixed by the project; a non-zero value is only honoured
        // by doubles and creates no normative API.
        std::uint64_t maxTimerWakeupUs;
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

    // Opt-in to deep sleep for a battery-powered client. Accepted only once and
    // only while Configuring; the configuration is copied and no resource is
    // started before setup(). Absence of the call, or enabled=false, preserves
    // the current runtime and touches neither GPIO nor wakeup sources.
    AppResult configureDeepSleep(const app::DeepSleepConfig &config);

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
    // private member; it names no protocol or transport type. It also covers
    // the statically allocated stack of the private power-lifecycle task, which
    // exists in every composition even when deep sleep is not configured,
    // because the project allocates task stacks statically.
    static constexpr std::size_t kImplStorageBytes = 16384;

private:
    Impl &impl();
    const Impl &impl() const;

    alignas(alignof(std::max_align_t)) unsigned char implStorage_[kImplStorageBytes];
};

} // namespace iotsmartsys
