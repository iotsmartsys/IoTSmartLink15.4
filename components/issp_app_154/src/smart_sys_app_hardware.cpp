// Real platform/network/device/executor steps for iotsmartsys::SmartSysApp,
// and the production single-argument constructor that wires them as the
// default SetupHooks. This is the only file in issp_app_154 that includes
// issp_transport_154 or the reset services, and the only reason the
// component privately requires issp_transport_154 and nvs_flash: it is
// compiled solely for targets with an IEEE 802.15.4 radio (see
// components/issp_app_154/CMakeLists.txt). Automated tests never link this
// file: they use the SetupHooks-based constructor with fakes instead
// (components/issp_app_154/test_apps/smart_sys_app_test), so the state
// machine in smart_sys_app.cpp can be exercised without hardware.

#include "smart_sys_app_impl.hpp"

#include <new>

#include "esp_log.h"
#include "esp_mac.h"
#include "issp154_network_manager.hpp"
#include "issp154_report_executor.hpp"
#include "issp154_transport.hpp"
#include "issp_device.hpp"
#include "nvs_flash.h"
#include "reset/factory_reset_service.hpp"
#include "reset/reset_button_monitor.hpp"

namespace iotsmartsys
{

namespace
{

constexpr std::uint16_t kShortAddress = 0x1001;

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

// The concrete layout behind SmartSysApp::Impl::hardwareStorage_. Only this
// translation unit constructs, reads or destroys it.
struct HardwareState
{
    std::array<std::uint8_t, issp::kIssp154ExtendedAddressSize> extendedAddress;
    std::optional<issp::Issp154Transport> transport;
    std::optional<issp::Issp154NetworkManager> networkManager;
    std::optional<issp::IsspDevice> device;
    std::optional<issp::Issp154ReportExecutor> reportExecutor;
    std::optional<FactoryResetService> factoryResetService;
    std::optional<ResetButtonMonitor> resetButtonMonitor;
};

static_assert(sizeof(HardwareState) <= SmartSysApp::Impl::kHardwareStorageBytes,
             "SmartSysApp::Impl::kHardwareStorageBytes is too small for HardwareState");
static_assert(alignof(HardwareState) <= alignof(std::max_align_t),
             "HardwareState is overaligned for hardwareStorage_");

HardwareState &hardwareOf(SmartSysApp::Impl *impl)
{
    return *std::launder(reinterpret_cast<HardwareState *>(impl->hardwareStorage_));
}

esp_err_t clearNetworkConfiguration(void *context)
{
    if (context == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }
    auto *impl = static_cast<SmartSysApp::Impl *>(context);
    const issp::IsspResult result =
        hardwareOf(impl).networkManager->clearPersistedNetwork();
    return result == issp::IsspResult::Ok ? ESP_OK : ESP_FAIL;
}

} // namespace

AppResult SmartSysApp::Impl::realInitializePlatform(void *context)
{
    auto *self = static_cast<Impl *>(context);
    HardwareState &hardware = *new (self->hardwareStorage_) HardwareState{};

    const esp_err_t nvsResult = initializeNvs();
    if (nvsResult != ESP_OK)
    {
        return AppResult::Failed;
    }

    const esp_err_t macResult =
        esp_read_mac(hardware.extendedAddress.data(), ESP_MAC_IEEE802154);
    if (macResult != ESP_OK)
    {
        return AppResult::Failed;
    }

    const issp::Issp154TransportConfig transportConfig = {
        .channel = 0,
        .panId = 0,
        .shortAddress = kShortAddress,
        .coordinator = false,
        .extendedAddress = hardware.extendedAddress.data(),
        .cca = true,
        .promiscuous = false,
    };
    hardware.transport.emplace(transportConfig);
    hardware.networkManager.emplace(*hardware.transport, self->config_.deviceId);
    hardware.device.emplace(issp::IsspDeviceConfig{self->config_.deviceId}, *hardware.transport);
    hardware.reportExecutor.emplace(*hardware.device, *hardware.transport);

    if (self->factoryResetConfigured_)
    {
        hardware.factoryResetService.emplace(&clearNetworkConfiguration, self);
        const ResetButtonConfig resetConfig = {
            .gpio = self->factoryResetConfig_.pin,
            .holdTimeMs = self->factoryResetConfig_.holdTimeMs,
            .pollIntervalMs = self->factoryResetConfig_.pollIntervalMs,
            .activeLow = self->factoryResetConfig_.activeLow,
        };
        hardware.resetButtonMonitor.emplace(resetConfig, *hardware.factoryResetService);
        const esp_err_t resetStartResult = hardware.resetButtonMonitor->start();
        if (resetStartResult != ESP_OK)
        {
            return AppResult::Failed;
        }
    }

    return AppResult::Ok;
}

AppResult SmartSysApp::Impl::realInitializeNetwork(void *context)
{
    auto *self = static_cast<Impl *>(context);
    return mapIsspResult(hardwareOf(self).networkManager->initializeNetwork());
}

AppResult SmartSysApp::Impl::realRegisterCapability(void *context, std::size_t index)
{
    auto *self = static_cast<Impl *>(context);
    return mapIsspResult(
        hardwareOf(self).device->addBehavior(*self->switchBehaviors_[index]));
}

AppResult SmartSysApp::Impl::realStartDevice(void *context)
{
    auto *self = static_cast<Impl *>(context);
    return mapIsspResult(hardwareOf(self).device->start());
}

AppResult SmartSysApp::Impl::realStartReportExecutor(void *context)
{
    auto *self = static_cast<Impl *>(context);
    return mapIsspResult(hardwareOf(self).reportExecutor->start());
}

void SmartSysApp::Impl::realRollbackTransport(void *context)
{
    auto *self = static_cast<Impl *>(context);
    HardwareState &hardware = hardwareOf(self);
    if (!hardware.transport.has_value())
    {
        return;
    }
    const issp::IsspResult result = hardware.transport->end();
    ESP_LOGI("SmartSysApp", "app_setup rollback transport=%u",
             static_cast<unsigned>(result));
}

SmartSysApp::SmartSysApp(const app::SmartSysAppConfig &config)
{
    static_assert(sizeof(Impl) <= kImplStorageBytes,
                 "SmartSysApp::kImplStorageBytes is too small for Impl");
    static_assert(alignof(Impl) <= alignof(std::max_align_t),
                 "Impl is overaligned for implStorage_");
    new (implStorage_) Impl(config, nullptr);
}

} // namespace iotsmartsys
