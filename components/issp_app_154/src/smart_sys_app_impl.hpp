#pragma once

// Private, target-agnostic declaration of SmartSysApp::Impl, shared by
// smart_sys_app.cpp (state machine core, buildable on any target because it
// only needs issp_core/issp_behaviors for capability storage) and
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
#include <cstddef>
#include <optional>

#include "SmartSysApp.h"
#include "digital_input_behavior.hpp"
#include "digital_output_behavior.hpp"
#include "idevice_behavior.hpp"
#include "issp_limits.hpp"

namespace iotsmartsys
{

struct SmartSysApp::Impl
{
    Impl(const app::SmartSysAppConfig &config, const SetupHooks *hooksOverride);

    core::SwitchPlugCapability *addSwitchPlugCapability(const app::SwitchConfig &config);
    core::DoorSensorCapability *addDoorSensorCapability(const app::DoorSensorConfig &config);
    AppResult configureFactoryResetButton(const app::PushButtonConfig &config);
    SetupResult setup();

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
    bool hasDuplicateEndpoint(std::uint8_t endpointId, std::uint8_t eventType) const;
    SetupResult fail(SetupStage stage, AppResult result);

    // Defined only in smart_sys_app_hardware.cpp (hardware-capable targets).
    static AppResult realInitializePlatform(void *context);
    static AppResult realInitializeNetwork(void *context);
    static AppResult realRegisterCapability(void *context, std::size_t index);
    static AppResult realStartDevice(void *context);
    static AppResult realStartReportExecutor(void *context);
    static void realRollbackTransport(void *context);

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

    bool factoryResetConfigured_;
    app::PushButtonConfig factoryResetConfig_;
};

} // namespace iotsmartsys
