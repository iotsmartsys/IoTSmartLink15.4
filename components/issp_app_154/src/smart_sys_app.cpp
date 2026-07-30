#include "SmartSysApp.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"

namespace iotsmartsys
{

namespace
{

constexpr char kTag[] = "SmartSysApp";

AppResult mapIsspResult(issp::IsspResult result)
{
    switch (result)
    {
    case issp::IsspResult::Ok:
        return AppResult::Ok;
    case issp::IsspResult::InvalidArgument:
        return AppResult::InvalidArgument;
    case issp::IsspResult::NotReady:
        return AppResult::NotReady;
    case issp::IsspResult::Busy:
        return AppResult::Busy;
    case issp::IsspResult::Failed:
        return AppResult::Failed;
    }
    return AppResult::Failed;
}

esp_err_t initializeNvs()
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
        result == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        const esp_err_t eraseResult = nvs_flash_erase();
        if (eraseResult != ESP_OK)
        {
            return eraseResult;
        }
        result = nvs_flash_init();
    }
    return result;
}

} // namespace

namespace core
{

SwitchPlugCapability::SwitchPlugCapability(issp::DigitalOutputBehavior &behavior)
    : behavior_(&behavior)
{
}

bool SwitchPlugCapability::state() const
{
    return behavior_->state();
}

} // namespace core

SmartSysApp::SmartSysApp(const app::SmartSysAppConfig &config)
    : config_(config),
      state_(AppState::Configuring),
      lastSetupResult_{AppState::Configuring, SetupStage::None, AppResult::Ok},
      lastConfigurationResult_(AppResult::Ok),
      switchConfigs_{},
      switchBehaviors_{},
      switchCapabilities_{},
      switchCount_(0),
      factoryResetConfigured_(false),
      factoryResetConfig_{},
      extendedAddress_{}
{
    if (config_.deviceId == 0)
    {
        recordConfigurationFailure(AppResult::InvalidArgument);
    }
}

void SmartSysApp::recordConfigurationFailure(AppResult result)
{
    if (lastConfigurationResult_ == AppResult::Ok)
    {
        lastConfigurationResult_ = result;
    }
}

bool SmartSysApp::hasDuplicateSwitchEndpoint(const app::SwitchConfig &config) const
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
SmartSysApp::addSwitchPlugCapability(const app::SwitchConfig &config)
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
    switchCapabilities_[switchCount_].emplace(*switchBehaviors_[switchCount_]);
    core::SwitchPlugCapability *capability = &*switchCapabilities_[switchCount_];
    ++switchCount_;
    return capability;
}

AppResult
SmartSysApp::configureFactoryResetButton(const app::PushButtonConfig &config)
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

esp_err_t SmartSysApp::clearNetworkConfiguration(void *context)
{
    if (context == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }
    auto *self = static_cast<SmartSysApp *>(context);
    const issp::IsspResult result = self->networkManager_->clearPersistedNetwork();
    return result == issp::IsspResult::Ok ? ESP_OK : ESP_FAIL;
}

SetupResult SmartSysApp::fail(SetupStage stage, AppResult result)
{
    state_ = AppState::Failed;
    lastSetupResult_ = {AppState::Failed, stage, result};
    ESP_LOGE(kTag, "app_setup failed stage=%u result=%u",
             static_cast<unsigned>(stage), static_cast<unsigned>(result));
    return lastSetupResult_;
}

void SmartSysApp::shutdownTransportAfterFailure()
{
    if (!transport_.has_value())
    {
        return;
    }
    const issp::IsspResult result = transport_->end();
    ESP_LOGI(kTag, "app_setup rollback transport=%u",
             static_cast<unsigned>(result));
}

SetupResult SmartSysApp::setup()
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

    const esp_err_t nvsResult = initializeNvs();
    if (nvsResult != ESP_OK)
    {
        return fail(SetupStage::InitializePlatform, AppResult::Failed);
    }

    const esp_err_t macResult =
        esp_read_mac(extendedAddress_.data(), ESP_MAC_IEEE802154);
    if (macResult != ESP_OK)
    {
        return fail(SetupStage::InitializePlatform, AppResult::Failed);
    }

    const issp::Issp154TransportConfig transportConfig = {
        .channel = 0,
        .panId = 0,
        .shortAddress = kShortAddress,
        .coordinator = false,
        .extendedAddress = extendedAddress_.data(),
        .cca = true,
        .promiscuous = false,
    };
    transport_.emplace(transportConfig);
    networkManager_.emplace(*transport_, config_.deviceId);
    device_.emplace(issp::IsspDeviceConfig{config_.deviceId}, *transport_);
    reportExecutor_.emplace(*device_, *transport_);

    if (factoryResetConfigured_)
    {
        factoryResetService_.emplace(&SmartSysApp::clearNetworkConfiguration, this);
        const ResetButtonConfig resetConfig = {
            .gpio = factoryResetConfig_.pin,
            .holdTimeMs = factoryResetConfig_.holdTimeMs,
            .pollIntervalMs = factoryResetConfig_.pollIntervalMs,
            .activeLow = factoryResetConfig_.activeLow,
        };
        resetButtonMonitor_.emplace(resetConfig, *factoryResetService_);
        const esp_err_t resetStartResult = resetButtonMonitor_->start();
        if (resetStartResult != ESP_OK)
        {
            return fail(SetupStage::InitializePlatform, AppResult::Failed);
        }
    }

    ESP_LOGI(kTag, "app_setup stage=%u",
             static_cast<unsigned>(SetupStage::InitializeNetwork));
    const issp::IsspResult networkResult = networkManager_->initializeNetwork();
    if (networkResult != issp::IsspResult::Ok)
    {
        shutdownTransportAfterFailure();
        if (networkResult == issp::IsspResult::NotReady)
        {
            state_ = AppState::NotReady;
            lastSetupResult_ = {AppState::NotReady, SetupStage::InitializeNetwork,
                                AppResult::NotReady};
            ESP_LOGW(kTag, "app_setup completed state=not_ready stage=%u result=%u",
                     static_cast<unsigned>(SetupStage::InitializeNetwork),
                     static_cast<unsigned>(AppResult::NotReady));
            return lastSetupResult_;
        }
        return fail(SetupStage::InitializeNetwork, mapIsspResult(networkResult));
    }

    ESP_LOGI(kTag, "app_setup stage=%u",
             static_cast<unsigned>(SetupStage::RegisterCapabilities));
    for (std::size_t index = 0; index < switchCount_; ++index)
    {
        const issp::IsspResult addResult =
            device_->addBehavior(*switchBehaviors_[index]);
        if (addResult != issp::IsspResult::Ok)
        {
            shutdownTransportAfterFailure();
            return fail(SetupStage::RegisterCapabilities, mapIsspResult(addResult));
        }
    }

    ESP_LOGI(kTag, "app_setup stage=%u",
             static_cast<unsigned>(SetupStage::StartDevice));
    const issp::IsspResult deviceResult = device_->start();
    if (deviceResult != issp::IsspResult::Ok)
    {
        shutdownTransportAfterFailure();
        return fail(SetupStage::StartDevice, mapIsspResult(deviceResult));
    }

    ESP_LOGI(kTag, "app_setup stage=%u",
             static_cast<unsigned>(SetupStage::StartReportExecutor));
    const issp::IsspResult executorResult = reportExecutor_->start();
    if (executorResult != issp::IsspResult::Ok)
    {
        shutdownTransportAfterFailure();
        return fail(SetupStage::StartReportExecutor, mapIsspResult(executorResult));
    }

    state_ = AppState::Running;
    lastSetupResult_ = {AppState::Running, SetupStage::Completed, AppResult::Ok};
    ESP_LOGI(kTag, "app_setup completed state=running");
    return lastSetupResult_;
}

AppState SmartSysApp::state() const
{
    return state_;
}

SetupResult SmartSysApp::lastSetupResult() const
{
    return lastSetupResult_;
}

AppResult SmartSysApp::lastConfigurationResult() const
{
    return lastConfigurationResult_;
}

std::uint32_t SmartSysApp::deviceId() const
{
    return config_.deviceId;
}

} // namespace iotsmartsys
