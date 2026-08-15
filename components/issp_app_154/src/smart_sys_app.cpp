// Target-agnostic core of iotsmartsys::SmartSysApp: configuration
// validation and the setup() state machine. It never calls NVS, GPIO
// drivers or radio APIs directly -- every hardware-touching step goes
// through SmartSysApp::SetupHooks, wired to the real implementations in
// smart_sys_app_hardware.cpp or to fakes supplied by automated tests. This
// file only needs issp_core/issp_behaviors (peripheral types, no radio), so its logic is
// exercised by the fake-backed Unity app on a physical ESP32-H2 without
// starting the radio.

#include "smart_sys_app_impl.hpp"

#include <new>

#include "esp_log.h"
#include "sdkconfig.h"

namespace iotsmartsys
{

namespace
{

constexpr char kTag[] = "SmartSysApp";

bool switchStateThunk(void *context)
{
    return static_cast<issp::DigitalOutputBehavior *>(context)->state();
}

bool doorSensorStateThunk(void *context)
{
    return static_cast<issp::DigitalInputBehavior *>(context)->state();
}

issp::DigitalInputPull mapInputPull(app::DigitalInputPull pull)
{
    switch (pull)
    {
    case app::DigitalInputPull::Floating:
        return issp::DigitalInputPull::Floating;
    case app::DigitalInputPull::PullUp:
        return issp::DigitalInputPull::PullUp;
    case app::DigitalInputPull::PullDown:
        return issp::DigitalInputPull::PullDown;
    }
    return issp::DigitalInputPull::Floating;
}

} // namespace

namespace core
{

SwitchPlugCapability::SwitchPlugCapability(StateFn stateFn, void *context)
    : stateFn_(stateFn), context_(context)
{
}

bool SwitchPlugCapability::state() const
{
    return stateFn_(context_);
}

DoorSensorCapability::DoorSensorCapability(StateFn stateFn, void *context)
    : stateFn_(stateFn), context_(context)
{
}

bool DoorSensorCapability::state() const
{
    return stateFn_(context_);
}

} // namespace core

SmartSysApp::Impl::Impl(const app::SmartSysAppConfig &config,
                        const SetupHooks *hooksOverride)
    : hardwareStorage_{},
      config_(config),
      state_(AppState::Configuring),
      lastSetupResult_{AppState::Configuring, SetupStage::None, AppResult::Ok},
      lastConfigurationResult_(AppResult::Ok),
      hooks_{},
      behaviors_{},
      endpointEventPairs_{},
      behaviorCount_(0),
      switchConfigs_{},
      switchBehaviors_{},
      switchCapabilities_{},
      switchCount_(0),
      doorSensorConfigs_{},
      doorSensorBehaviors_{},
      doorSensorCapabilities_{},
      doorSensorCount_(0),
      batteryConfigs_{},
      batteryBehaviors_{},
      batteryCapabilities_{},
      batteryCount_(0),
      factoryResetConfigured_(false),
      factoryResetConfig_{},
      deepSleepConfigured_(false),
      deepSleepConfig_{},
      awakeWindowStartUs_(0),
      currentStage_(SetupStage::None),
      transitionOwner_(static_cast<std::uint8_t>(TransitionOwner::Free)),
      wakeLedOn_(false),
      wakeLedTimer_(nullptr),
      lifecycleTaskControl_{},
      lifecycleTaskStack_{},
      lifecycleTaskHandle_(nullptr)
{
    if (hooksOverride != nullptr)
    {
        hooks_ = *hooksOverride;
    }
#if defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C6)
    else
    {
        // Only SmartSysApp's single-argument constructor
        // (smart_sys_app_hardware.cpp, compiled solely for these targets)
        // ever passes hooksOverride == nullptr.
        hooks_.initializePlatform = &Impl::realInitializePlatform;
        hooks_.initializeNetwork = &Impl::realInitializeNetwork;
        hooks_.registerCapability = &Impl::realRegisterCapability;
        hooks_.startDevice = &Impl::realStartDevice;
        hooks_.startReportExecutor = &Impl::realStartReportExecutor;
        hooks_.rollbackTransport = &Impl::realRollbackTransport;
        hooks_.context = this;
        hooks_.beginDeviceQuiescence = &Impl::realBeginDeviceQuiescence;
        hooks_.stopReportExecutor = &Impl::realStopReportExecutor;
        hooks_.endTransport = &Impl::realEndTransport;
        hooks_.pendingReportCount = &Impl::realPendingReportCount;
        hooks_.stopResetButtonMonitor = &Impl::realStopResetButtonMonitor;
    }
#endif

    if (config_.deviceId == 0)
    {
        recordConfigurationFailure(AppResult::InvalidArgument);
    }
}

void SmartSysApp::Impl::recordConfigurationFailure(AppResult result)
{
    if (lastConfigurationResult_ == AppResult::Ok)
    {
        lastConfigurationResult_ = result;
    }
}

bool SmartSysApp::Impl::hasDuplicateEndpoint(std::uint8_t endpointId,
                                             std::uint8_t eventType) const
{
    for (std::size_t index = 0; index < behaviorCount_; ++index)
    {
        if (endpointEventPairs_[index].endpointId == endpointId &&
            endpointEventPairs_[index].eventType == eventType)
        {
            return true;
        }
    }
    return false;
}

bool SmartSysApp::Impl::hasOccupiedEndpoint(std::uint8_t endpointId) const
{
    for (std::size_t index = 0; index < behaviorCount_; ++index)
    {
        if (endpointEventPairs_[index].endpointId == endpointId)
        {
            return true;
        }
    }
    return false;
}

core::SwitchPlugCapability *
SmartSysApp::Impl::addSwitchPlugCapability(const app::SwitchConfig &config)
{
    if (state_ != AppState::Configuring)
    {
        recordConfigurationFailure(AppResult::Failed);
        return nullptr;
    }
    if (!GPIO_IS_VALID_OUTPUT_GPIO(config.pin))
    {
        recordConfigurationFailure(AppResult::InvalidArgument);
        return nullptr;
    }
    if (hasDuplicateEndpoint(config.endpointId, config.eventType))
    {
        recordConfigurationFailure(AppResult::InvalidArgument);
        return nullptr;
    }
    if (collidesWithWakeLed(config.pin))
    {
        recordConfigurationFailure(AppResult::InvalidArgument);
        return nullptr;
    }
    if (behaviorCount_ >= kMaxCapabilities || switchCount_ >= kMaxCapabilities)
    {
        recordConfigurationFailure(AppResult::Failed);
        return nullptr;
    }

    const issp::DigitalOutputConfig behaviorConfig = {
        .endpointId = config.endpointId,
        .eventType = config.eventType,
        .pin = config.pin,
        .activeLevel = config.activeHigh ? 1U : 0U,
        .initialState = config.initialState,
        .reportOnStart = config.reportOnStart,
    };

    switchConfigs_[switchCount_] = config;
    switchBehaviors_[switchCount_].emplace(behaviorConfig);
    switchCapabilities_[switchCount_].emplace(
        &switchStateThunk, static_cast<void *>(&*switchBehaviors_[switchCount_]));
    core::SwitchPlugCapability *capability = &*switchCapabilities_[switchCount_];
    behaviors_[behaviorCount_] = &*switchBehaviors_[switchCount_];
    endpointEventPairs_[behaviorCount_] = {config.endpointId, config.eventType};
    ++behaviorCount_;
    ++switchCount_;
    return capability;
}

core::DoorSensorCapability *
SmartSysApp::Impl::addDoorSensorCapability(const app::DoorSensorConfig &config)
{
    if (state_ != AppState::Configuring)
    {
        recordConfigurationFailure(AppResult::Failed);
        return nullptr;
    }
    const bool validPull = config.pull == app::DigitalInputPull::Floating ||
                           config.pull == app::DigitalInputPull::PullUp ||
                           config.pull == app::DigitalInputPull::PullDown;
    if (!GPIO_IS_VALID_GPIO(config.pin) || !validPull ||
        config.samplePeriodMs == 0U ||
        config.samplesPerWindow == 0U || config.majorityThreshold == 0U ||
        config.majorityThreshold > config.samplesPerWindow ||
        config.consecutiveWindows == 0U)
    {
        recordConfigurationFailure(AppResult::InvalidArgument);
        return nullptr;
    }
    if (hasDuplicateEndpoint(config.endpointId, config.eventType))
    {
        recordConfigurationFailure(AppResult::InvalidArgument);
        return nullptr;
    }
    if (collidesWithWakeLed(config.pin))
    {
        recordConfigurationFailure(AppResult::InvalidArgument);
        return nullptr;
    }
    if (behaviorCount_ >= kMaxCapabilities || doorSensorCount_ >= kMaxCapabilities)
    {
        recordConfigurationFailure(AppResult::Failed);
        return nullptr;
    }

    const issp::DigitalInputConfig behaviorConfig = {
        .endpointId = config.endpointId,
        .eventType = config.eventType,
        .pin = config.pin,
        .activeLevel = config.activeHigh ? 1U : 0U,
        .pull = mapInputPull(config.pull),
        .reportOnStart = config.reportOnStart,
        .samplePeriodMs = config.samplePeriodMs,
        .samplesPerWindow = config.samplesPerWindow,
        .majorityThreshold = config.majorityThreshold,
        .consecutiveWindows = config.consecutiveWindows,
    };

    doorSensorConfigs_[doorSensorCount_] = config;
    doorSensorBehaviors_[doorSensorCount_].emplace(behaviorConfig);
    doorSensorCapabilities_[doorSensorCount_].emplace(
        &doorSensorStateThunk,
        static_cast<void *>(&*doorSensorBehaviors_[doorSensorCount_]));
    core::DoorSensorCapability *capability =
        &*doorSensorCapabilities_[doorSensorCount_];
    behaviors_[behaviorCount_] = &*doorSensorBehaviors_[doorSensorCount_];
    endpointEventPairs_[behaviorCount_] = {config.endpointId, config.eventType};
    ++behaviorCount_;
    ++doorSensorCount_;
    return capability;
}

core::BatteryLevelCapability *
SmartSysApp::Impl::addBatteryLevelCapability(const app::BatteryLevelConfig &config)
{
    if (state_ != AppState::Configuring)
    {
        recordConfigurationFailure(AppResult::Failed);
        return nullptr;
    }
    if (config.samples == 0U || config.reportDeltaPercent == 0U ||
        config.reportDeltaPercent > 100U || config.fullMv <= config.emptyMv ||
        config.rBottomOhms == 0U || config.endpointId == 0U)
    {
        recordConfigurationFailure(AppResult::InvalidArgument);
        return nullptr;
    }
    // ADR-0005 applies in the implementable direction contracted by v0.5: the
    // new operation rejects every endpoint already occupied, regardless of
    // event type. Existing registration operations retain their known debt.
    if (hasOccupiedEndpoint(config.endpointId))
    {
        recordConfigurationFailure(AppResult::InvalidArgument);
        return nullptr;
    }
    if (behaviorCount_ >= kMaxCapabilities || batteryCount_ >= kMaxCapabilities)
    {
        recordConfigurationFailure(AppResult::Failed);
        return nullptr;
    }

    const issp::BatteryLevelConfig behaviorConfig = {
        .unit = config.unit,
        .channel = config.channel,
        .attenuation = config.attenuation,
        .rTopOhms = config.rTopOhms,
        .rBottomOhms = config.rBottomOhms,
        .emptyMv = config.emptyMv,
        .fullMv = config.fullMv,
        .samples = config.samples,
        .sampleIntervalMs = config.sampleIntervalMs,
        .samplePeriodMs = config.samplePeriodMs,
        .reportDeltaPercent = config.reportDeltaPercent,
        .endpointId = config.endpointId,
    };

    batteryConfigs_[batteryCount_] = config;
    batteryBehaviors_[batteryCount_].emplace(behaviorConfig);
    batteryCapabilities_[batteryCount_].emplace();
    core::BatteryLevelCapability *capability =
        &*batteryCapabilities_[batteryCount_];
    behaviors_[behaviorCount_] = &*batteryBehaviors_[batteryCount_];
    endpointEventPairs_[behaviorCount_] = {
        config.endpointId, issp::BatteryLevelBehavior::kEventType};
    ++behaviorCount_;
    ++batteryCount_;
    return capability;
}

AppResult SmartSysApp::Impl::validateBatterySampling() const
{
    const bool deepSleepEnabled = deepSleepConfigured_ && deepSleepConfig_.enabled;
    for (std::size_t index = 0; index < batteryCount_; ++index)
    {
        const bool periodic = batteryConfigs_[index].samplePeriodMs != 0U;
        if (periodic == deepSleepEnabled)
        {
            return AppResult::InvalidArgument;
        }
    }
    return AppResult::Ok;
}

AppResult
SmartSysApp::Impl::configureFactoryResetButton(const app::PushButtonConfig &config)
{
    if (state_ != AppState::Configuring)
    {
        recordConfigurationFailure(AppResult::Failed);
        return AppResult::Failed;
    }
    if (factoryResetConfigured_)
    {
        recordConfigurationFailure(AppResult::Failed);
        return AppResult::Failed;
    }
    if (!GPIO_IS_VALID_GPIO(config.pin) || config.holdTimeMs == 0U ||
        config.pollIntervalMs == 0U || collidesWithWakeLed(config.pin))
    {
        recordConfigurationFailure(AppResult::InvalidArgument);
        return AppResult::InvalidArgument;
    }

    factoryResetConfig_ = config;
    factoryResetConfigured_ = true;
    return AppResult::Ok;
}

SetupResult SmartSysApp::Impl::fail(SetupStage stage, AppResult result)
{
    state_ = AppState::Failed;
    lastSetupResult_ = {AppState::Failed, stage, result};
    ESP_LOGE(kTag, "app_setup failed stage=%u result=%u",
             static_cast<unsigned>(stage), static_cast<unsigned>(result));
    return lastSetupResult_;
}

SetupResult SmartSysApp::Impl::setup()
{
    if (state_ != AppState::Configuring)
    {
        return SetupResult{state_, SetupStage::None, AppResult::Busy};
    }

    // Start of the awake window. It precedes every stage because the deadline of
    // maxAwakeTimeMs is absolute and counted from setup().
    awakeWindowStartUs_ = esp_timer_get_time();

    ESP_LOGI(kTag, "app_setup begin capabilities=%u factory_reset=%s deep_sleep=%s",
             static_cast<unsigned>(behaviorCount_),
             factoryResetConfigured_ ? "configured" : "disabled",
             deepSleepConfigured_ && deepSleepConfig_.enabled ? "enabled" : "disabled");

    currentStage_ = SetupStage::ValidateConfiguration;
    ESP_LOGI(kTag, "app_setup stage=%u",
             static_cast<unsigned>(SetupStage::ValidateConfiguration));
    if (lastConfigurationResult_ != AppResult::Ok)
    {
        // Nothing is touched here: no NVS, radio, RTC or GPIO, so this boot has
        // no LED, no lifecycle task and no deep sleep.
        return fail(SetupStage::ValidateConfiguration, lastConfigurationResult_);
    }
    // Delayed until setup() so registering the battery before or after deep
    // sleep configuration remains equivalent while the facade is Configuring.
    const AppResult batteryResult = validateBatterySampling();
    if (batteryResult != AppResult::Ok)
    {
        recordConfigurationFailure(batteryResult);
        return fail(SetupStage::ValidateConfiguration, batteryResult);
    }
    // Checked only here, so the order between registering the dry-contact
    // capability and configureDeepSleep() stays insignificant while Configuring.
    const AppResult contactResult = validateContactWakeup();
    if (contactResult != AppResult::Ok)
    {
        recordConfigurationFailure(contactResult);
        return fail(SetupStage::ValidateConfiguration, contactResult);
    }

    state_ = AppState::Starting;

    currentStage_ = SetupStage::InitializePlatform;
    ESP_LOGI(kTag, "app_setup stage=%u",
             static_cast<unsigned>(SetupStage::InitializePlatform));
    // First platform operation of the stage: boot cause and LED, before NVS,
    // commissioning, radio or reports.
    AppResult result = beginPlatformPowerPolicy();
    if (result != AppResult::Ok)
    {
        return fail(SetupStage::InitializePlatform, result);
    }
    result = hooks_.initializePlatform(hooks_.context);
    if (result != AppResult::Ok)
    {
        return fail(SetupStage::InitializePlatform, result);
    }
    // Every mandatory resource exists and the optional monitor was handled: the
    // stage is not preemptible, so the lifecycle task is born only now.
    startPowerLifecycle();

    currentStage_ = SetupStage::InitializeNetwork;
    ESP_LOGI(kTag, "app_setup stage=%u",
             static_cast<unsigned>(SetupStage::InitializeNetwork));
    result = hooks_.initializeNetwork(hooks_.context);
    if (result != AppResult::Ok)
    {
        hooks_.rollbackTransport(hooks_.context);
        if (result == AppResult::NotReady)
        {
            state_ = AppState::NotReady;
            lastSetupResult_ = {AppState::NotReady, SetupStage::InitializeNetwork,
                                AppResult::NotReady};
            ESP_LOGW(kTag, "app_setup completed state=not_ready stage=%u result=%u",
                     static_cast<unsigned>(SetupStage::InitializeNetwork),
                     static_cast<unsigned>(AppResult::NotReady));
            return lastSetupResult_;
        }
        return fail(SetupStage::InitializeNetwork, result);
    }

    currentStage_ = SetupStage::RegisterCapabilities;
    ESP_LOGI(kTag, "app_setup stage=%u",
             static_cast<unsigned>(SetupStage::RegisterCapabilities));
    for (std::size_t index = 0; index < behaviorCount_; ++index)
    {
        result = hooks_.registerCapability(hooks_.context, index);
        if (result != AppResult::Ok)
        {
            hooks_.rollbackTransport(hooks_.context);
            return fail(SetupStage::RegisterCapabilities, result);
        }
    }

    currentStage_ = SetupStage::StartDevice;
    ESP_LOGI(kTag, "app_setup stage=%u",
             static_cast<unsigned>(SetupStage::StartDevice));
    result = hooks_.startDevice(hooks_.context);
    if (result != AppResult::Ok)
    {
        hooks_.rollbackTransport(hooks_.context);
        return fail(SetupStage::StartDevice, result);
    }

    currentStage_ = SetupStage::StartReportExecutor;
    ESP_LOGI(kTag, "app_setup stage=%u",
             static_cast<unsigned>(SetupStage::StartReportExecutor));
    result = hooks_.startReportExecutor(hooks_.context);
    if (result != AppResult::Ok)
    {
        hooks_.rollbackTransport(hooks_.context);
        return fail(SetupStage::StartReportExecutor, result);
    }

    currentStage_ = SetupStage::Completed;
    state_ = AppState::Running;
    lastSetupResult_ = {AppState::Running, SetupStage::Completed, AppResult::Ok};
    ESP_LOGI(kTag, "app_setup completed state=running");
    return lastSetupResult_;
}

SmartSysApp::SmartSysApp(const app::SmartSysAppConfig &config, const SetupHooks &hooks)
{
    static_assert(sizeof(Impl) <= kImplStorageBytes,
                 "SmartSysApp::kImplStorageBytes is too small for Impl");
    static_assert(alignof(Impl) <= alignof(std::max_align_t),
                 "Impl is overaligned for implStorage_");
    new (implStorage_) Impl(config, &hooks);
}

SmartSysApp::~SmartSysApp()
{
    impl().~Impl();
}

SmartSysApp::Impl &SmartSysApp::impl()
{
    return *std::launder(reinterpret_cast<Impl *>(implStorage_));
}

const SmartSysApp::Impl &SmartSysApp::impl() const
{
    return *std::launder(reinterpret_cast<const Impl *>(implStorage_));
}

core::SwitchPlugCapability *
SmartSysApp::addSwitchPlugCapability(const app::SwitchConfig &config)
{
    return impl().addSwitchPlugCapability(config);
}

core::DoorSensorCapability *
SmartSysApp::addDoorSensorCapability(const app::DoorSensorConfig &config)
{
    return impl().addDoorSensorCapability(config);
}

core::BatteryLevelCapability *
SmartSysApp::addBatteryLevelCapability(const app::BatteryLevelConfig &config)
{
    return impl().addBatteryLevelCapability(config);
}

AppResult SmartSysApp::configureFactoryResetButton(const app::PushButtonConfig &config)
{
    return impl().configureFactoryResetButton(config);
}

AppResult SmartSysApp::configureDeepSleep(const app::DeepSleepConfig &config)
{
    return impl().configureDeepSleep(config);
}

SetupResult SmartSysApp::setup()
{
    return impl().setup();
}

AppState SmartSysApp::state() const
{
    return impl().state();
}

SetupResult SmartSysApp::lastSetupResult() const
{
    return impl().lastSetupResult();
}

AppResult SmartSysApp::lastConfigurationResult() const
{
    return impl().lastConfigurationResult();
}

std::uint32_t SmartSysApp::deviceId() const
{
    return impl().deviceId();
}

} // namespace iotsmartsys
