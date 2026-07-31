#include <cstdint>

#include "SmartSysApp.h"
#include "esp_log.h"

static const char *TAG = "iot154_switch";

// The instance below is named "smartSysApp", not "app": with
// "using namespace iotsmartsys;" in scope, an "app" identifier would be
// ambiguous with the nested "iotsmartsys::app" namespace.
using namespace iotsmartsys;

namespace
{
constexpr gpio_num_t kRelayGpio = GPIO_NUM_13;
constexpr gpio_num_t kResetButtonGpio = GPIO_NUM_9;
constexpr std::uint32_t kDeviceId = 0x15400001;
constexpr std::uint8_t kRelayEndpointId = 1;
constexpr std::uint8_t kPowerEventType = 2;
}

static SmartSysApp smartSysApp({
    .deviceId = kDeviceId,
});

extern "C" void app_main()
{
    smartSysApp.addSwitchPlugCapability({
        .pin = kRelayGpio,
        .activeHigh = true,
        .initialState = false,
        .reportOnStart = true,
        .endpointId = kRelayEndpointId,
        .eventType = kPowerEventType,
    });

    smartSysApp.configureFactoryResetButton({
        .pin = kResetButtonGpio,
        .activeLow = true,
        .holdTimeMs = 10000,
        .pollIntervalMs = 20,
    });

    const SetupResult result = smartSysApp.setup();
    if (result.state != AppState::Running)
    {
        ESP_LOGE(TAG, "ISSP runtime did not start: state=%u stage=%u result=%u",
                 static_cast<unsigned>(result.state),
                 static_cast<unsigned>(result.stage),
                 static_cast<unsigned>(result.result));
        return;
    }

    ESP_LOGI(TAG, "ISSP runtime started");
}
