#include "esp_log.h"
#include "product_firmware.hpp"

static const char *TAG = "iot154_client";

// The entrypoint holds no product rule: it starts the composition selected in
// the menuconfig and reports the outcome. Identity, pins, endpoints and
// capabilities belong to firmwares/ and boards/.
extern "C" void app_main()
{
    const iotsmartsys::SetupResult result = client154::startSelectedProductFirmware();
    if (result.state != iotsmartsys::AppState::Running)
    {
        ESP_LOGE(TAG, "ISSP runtime did not start: state=%u stage=%u result=%u",
                 static_cast<unsigned>(result.state),
                 static_cast<unsigned>(result.stage),
                 static_cast<unsigned>(result.result));
        return;
    }

    ESP_LOGI(TAG, "ISSP runtime started");
}
