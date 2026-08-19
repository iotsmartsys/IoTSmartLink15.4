#pragma once

// Private, target-agnostic declaration of SmartSysApp::Impl, shared by
// smart_sys_app.cpp (state machine core, buildable on any target because it
// only needs issp_core/issp_behaviors for capability storage),
// smart_sys_app_deep_sleep.cpp (the deep sleep policy, equally target-agnostic
// because it reaches the runtime only through SetupHooks) and
// smart_sys_app_hardware.cpp (the real platform/network/device/executor
// hooks and the production single-argument SmartSysApp constructor, which
// need issp_transport_154; every admitted target carries an IEEE 802.15.4
// radio, so it is always compiled). Hardware-backed objects (transport, network
// manager, device, report executor, factory reset service/monitor) are
// never embedded in this struct: they live behind hardwareStorage_, an
// opaque fixed-size buffer only smart_sys_app_hardware.cpp knows how to
// interpret. This lets SMARTAPP-AC-006 to AC-013 be covered by automated
// tests on a physical ESP32-H2 without ever starting the radio stack.

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

#include "SmartSysApp.h"
#include "battery_level_behavior.hpp"
#include "battery_telemetry_state_behavior.hpp"
#include "digital_input_behavior.hpp"
#include "digital_output_behavior.hpp"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "idevice_behavior.hpp"
#include "issp_limits.hpp"

namespace iotsmartsys
{

struct SmartSysApp::Impl
{
    Impl(const app::SmartSysAppConfig &config, const SetupHooks *hooksOverride);

    core::SwitchPlugCapability *addSwitchPlugCapability(const app::SwitchConfig &config);
    core::DoorSensorCapability *addDoorSensorCapability(const app::DoorSensorConfig &config);
    core::BatteryLevelCapability *addBatteryLevelCapability(
        const app::BatteryLevelConfig &config);
    AppResult configureFactoryResetButton(const app::PushButtonConfig &config);
    AppResult configureDeepSleep(const app::DeepSleepConfig &config);
    SetupResult setup();
    void startDeferredBatterySampling();

    AppState state() const { return state_; }
    SetupResult lastSetupResult() const { return lastSetupResult_; }
    AppResult lastConfigurationResult() const { return lastConfigurationResult_; }
    std::uint32_t deviceId() const { return config_.deviceId; }

    static constexpr std::size_t kMaxCapabilities = issp::kMaxDeviceBehaviors;

    // Opaque storage for the hardware-backed objects (transport, network
    // manager, device, report executor, factory reset service/monitor,
    // extended address) that only smart_sys_app_hardware.cpp constructs and
    // reads, via reinterpret_cast against its own HardwareState definition.
    // Sized generously and verified there by a static_assert.
    static constexpr std::size_t kHardwareStorageBytes = 8192;
    alignas(alignof(std::max_align_t)) unsigned char hardwareStorage_[kHardwareStorageBytes];

    void recordConfigurationFailure(AppResult result);
    bool hasOccupiedEndpoint(std::uint8_t endpointId) const;
    AppResult validateBatterySampling() const;
    SetupResult fail(SetupStage stage, AppResult result);

    // --- deep sleep (specification docs/specs/Client-Deep-Sleep.md) ---
    //
    // Everything below is defined in smart_sys_app_deep_sleep.cpp, which is as
    // target-agnostic as smart_sys_app.cpp: it owns the policy (validation,
    // deadline, LED, readiness, ordered terminal sequence and the arbitration
    // with factory reset) and reaches the runtime objects only through
    // SetupHooks, so the lifecycle can be exercised with doubles.

    // The single power transition disputed by deep sleep and factory reset.
    enum class TransitionOwner : std::uint8_t
    {
        Free,
        DeepSleep,
        FactoryReset,
    };

    static constexpr std::uint32_t kLifecyclePollIntervalMs = 10;
    static constexpr std::uint32_t kReportExecutorStopBudgetMs = 600;
    static constexpr std::size_t kLifecycleTaskStackSizeBytes = 4096;
    static constexpr std::size_t kLifecycleTaskStackDepth =
        kLifecycleTaskStackSizeBytes / sizeof(StackType_t);

    /// Pure helper: converts the configured interval to microseconds. Configures
    /// no RTC and touches no hardware.
    static std::uint64_t timerWakeupIntervalUs(const app::TimerWakeupConfig &timer);
    /// Conservative upper bound accepted for the timer wakeup, derived from the
    /// target LP timer width and from the slow clock source fixed by the
    /// project. Honours an injected limit when the seam supplies one.
    std::uint64_t maxTimerWakeupUs() const;
    bool wakeLedEnabled() const;
    bool collidesWithWakeLed(gpio_num_t pin) const;

    bool contactWakeupEnabled() const;
    /// First dry-contact capability registered on the wakeup GPIO, or nullptr
    /// when none matches. Its registered configuration is what supplies the
    /// direction and the pull reapplied to the pad before arming.
    const app::DoorSensorConfig *matchingContactConfig() const;
    /// SetupStage::ValidateConfiguration check of the contact source: the GPIO
    /// must match a registered dry-contact capability, and capabilities sharing
    /// that GPIO must declare the same pull.
    AppResult validateContactWakeup() const;
    /// Reapplies the pad configuration, reads its electrical level and arms the
    /// external wakeup for the opposite one. Idempotent, and reached even when
    /// SetupStage::StartDevice was never completed.
    AppResult prepareContactWakeup();

    /// First platform operation of SetupStage::InitializePlatform: records the
    /// boot cause and configures and lights the LED, before NVS, commissioning,
    /// radio or reports.
    AppResult beginPlatformPowerPolicy();
    /// Creates the lifecycle task at the end of a successful
    /// SetupStage::InitializePlatform, when every mandatory resource exists.
    void startPowerLifecycle();

    AppResult configureWakeLed();
    void setWakeLedLevel(bool on);
    void releaseWakeLed();
    static void wakeLedExpired(void *context);

    static void powerLifecycleTask(void *context);
    void runPowerLifecycle();
    bool readyForEarlyQuiescence() const;
    std::int64_t remainingAwakeMs() const;
    /// Ordered terminal sequence of section 6.1. Returns false when it aborted
    /// before any terminal operation, leaving the runtime reachable.
    bool runTerminalSequence(bool forced);
    bool acquireDeepSleepTransition();
    void releaseDeepSleepTransition();
    /// Wired into FactoryResetService so the first accepted transition wins.
    static bool acquireFactoryResetTransition(void *context);
    static void releaseFactoryResetTransition(void *context);

    // Defined only in smart_sys_app_hardware.cpp (hardware-capable targets).
    static AppResult realInitializePlatform(void *context);
    static AppResult realInitializeNetwork(void *context);
    static AppResult realRegisterCapability(void *context, std::size_t index,
                                            std::uint8_t endpointId,
                                            std::uint8_t eventType);
    static AppResult realStartDevice(void *context);
    static AppResult realStartReportExecutor(void *context);
    static void realRollbackTransport(void *context);
    static AppResult realBeginDeviceQuiescence(void *context);
    static AppResult realStopReportExecutor(void *context);
    static void realEndTransport(void *context);
    static std::size_t realPendingReportCount(void *context);
    static void realStopResetButtonMonitor(void *context);

    app::SmartSysAppConfig config_;
    AppState state_;
    SetupResult lastSetupResult_;
    AppResult lastConfigurationResult_;
    SetupHooks hooks_;

    struct EndpointEventPair
    {
        std::uint8_t endpointId;
        std::uint8_t eventType;
    };

    std::array<issp::IDeviceBehavior *, kMaxCapabilities> behaviors_;
    std::array<EndpointEventPair, kMaxCapabilities> endpointEventPairs_;
    std::size_t behaviorCount_;

    std::array<app::SwitchConfig, kMaxCapabilities> switchConfigs_;
    std::array<std::optional<issp::DigitalOutputBehavior>, kMaxCapabilities>
        switchBehaviors_;
    std::array<std::optional<core::SwitchPlugCapability>, kMaxCapabilities>
        switchCapabilities_;
    std::size_t switchCount_;

    std::array<app::DoorSensorConfig, kMaxCapabilities> doorSensorConfigs_;
    std::array<std::optional<issp::DigitalInputBehavior>, kMaxCapabilities>
        doorSensorBehaviors_;
    std::array<std::optional<core::DoorSensorCapability>, kMaxCapabilities>
        doorSensorCapabilities_;
    std::size_t doorSensorCount_;

    std::array<app::BatteryLevelConfig, kMaxCapabilities> batteryConfigs_;
    std::array<std::optional<issp::BatteryLevelBehavior>, kMaxCapabilities>
        batteryBehaviors_;
    std::array<std::optional<core::BatteryLevelCapability>, kMaxCapabilities>
        batteryCapabilities_;
    std::array<std::optional<issp::BatteryTelemetryStateBehavior>, kMaxCapabilities>
        batteryStateBehaviors_;
    std::array<std::optional<core::BatteryTelemetryStateCapability>, kMaxCapabilities>
        batteryStateCapabilities_;
    std::size_t batteryCount_;

    bool factoryResetConfigured_;
    app::PushButtonConfig factoryResetConfig_;

    bool deepSleepConfigured_;
    app::DeepSleepConfig deepSleepConfig_;
    // Start of the awake window, captured on entry to setup().
    std::int64_t awakeWindowStartUs_;
    // Stage in progress, used only to diagnose that the deadline may have
    // preempted the private persistence window of initializeNetwork().
    SetupStage currentStage_;
    std::atomic<std::uint8_t> transitionOwner_;
    bool wakeLedOn_;
    esp_timer_handle_t wakeLedTimer_;
    StaticTask_t lifecycleTaskControl_;
    StackType_t lifecycleTaskStack_[kLifecycleTaskStackDepth];
    TaskHandle_t lifecycleTaskHandle_;
};

} // namespace iotsmartsys
