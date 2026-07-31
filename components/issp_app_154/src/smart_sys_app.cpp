// Target-agnostic core of iotsmartsys::SmartSysApp: configuration
// validation and the setup() state machine. It never calls NVS, GPIO
// drivers or radio APIs directly -- every hardware-touching step goes
// through SmartSysApp::SetupHooks, wired to the real implementations in
// smart_sys_app_hardware.cpp (compiled only for targets with an
// IEEE 802.15.4 radio) or to fakes supplied by automated tests. This file
// only needs issp_core/issp_behaviors (GPIO, no radio), so it builds and
// its logic can be exercised on any target, including one hosted by QEMU.

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

} // namespace core

SmartSysApp::Impl::Impl(const app::SmartSysAppConfig &config,
                        const SetupHooks *hooksOverride)
    : hardwareStorage_{},
      config_(config),
      state_(AppState::Configuring),
      lastSetupResult_{AppState::Configuring, SetupStage::None, AppResult::Ok},
      lastConfigurationResult_(AppResult::Ok),
      hooks_{},
      switchConfigs_{},
      switchBehaviors_{},
      switchCapabilities_{},
      switchCount_(0),
      factoryResetConfigured_(false),
      factoryResetConfig_{}
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

bool SmartSysApp::Impl::hasDuplicateSwitchEndpoint(const app::SwitchConfig &config) const
{
    for (std::size_t index = 0; index < switchCount_; ++index)
    {
        if (switchConfigs_[index].endpointId == config.endpointId &&
            switchConfigs_[index].eventType == config.eventType)
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
    if (hasDuplicateSwitchEndpoint(config))
    {
        recordConfigurationFailure(AppResult::InvalidArgument);
        return nullptr;
    }
    if (switchCount_ >= kMaxSwitchCapabilities)
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
    ++switchCount_;
    return capability;
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
        config.pollIntervalMs == 0U)
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

    ESP_LOGI(kTag, "app_setup begin capabilities=%u factory_reset=%s",
             static_cast<unsigned>(switchCount_),
             factoryResetConfigured_ ? "configured" : "disabled");

    ESP_LOGI(kTag, "app_setup stage=%u",
             static_cast<unsigned>(SetupStage::ValidateConfiguration));
    if (lastConfigurationResult_ != AppResult::Ok)
    {
        return fail(SetupStage::ValidateConfiguration, lastConfigurationResult_);
    }

    state_ = AppState::Starting;

    ESP_LOGI(kTag, "app_setup stage=%u",
             static_cast<unsigned>(SetupStage::InitializePlatform));
    AppResult result = hooks_.initializePlatform(hooks_.context);
    if (result != AppResult::Ok)
    {
        return fail(SetupStage::InitializePlatform, result);
    }

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

    ESP_LOGI(kTag, "app_setup stage=%u",
             static_cast<unsigned>(SetupStage::RegisterCapabilities));
    for (std::size_t index = 0; index < switchCount_; ++index)
    {
        result = hooks_.registerCapability(hooks_.context, index);
        if (result != AppResult::Ok)
        {
            hooks_.rollbackTransport(hooks_.context);
            return fail(SetupStage::RegisterCapabilities, result);
        }
    }

    ESP_LOGI(kTag, "app_setup stage=%u",
             static_cast<unsigned>(SetupStage::StartDevice));
    result = hooks_.startDevice(hooks_.context);
    if (result != AppResult::Ok)
    {
        hooks_.rollbackTransport(hooks_.context);
        return fail(SetupStage::StartDevice, result);
    }

    ESP_LOGI(kTag, "app_setup stage=%u",
             static_cast<unsigned>(SetupStage::StartReportExecutor));
    result = hooks_.startReportExecutor(hooks_.context);
    if (result != AppResult::Ok)
    {
        hooks_.rollbackTransport(hooks_.context);
        return fail(SetupStage::StartReportExecutor, result);
    }

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

AppResult SmartSysApp::configureFactoryResetButton(const app::PushButtonConfig &config)
{
    return impl().configureFactoryResetButton(config);
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
