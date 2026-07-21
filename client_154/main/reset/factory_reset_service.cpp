#include "factory_reset_service.hpp"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace
{

constexpr char kTag[] = "FACTORY_RESET";
constexpr TickType_t kLogFlushDelay = pdMS_TO_TICKS(50);

} // namespace

FactoryResetService::FactoryResetService()
    : requested_(false)
{
}

void FactoryResetService::requestFactoryReset()
{
    if (requested_)
    {
        return;
    }

    requested_ = true;
    ESP_LOGW(kTag, "requested");
    ESP_LOGW(kTag, "begin persistence_preserved=yes");

    // Persistence cleanup will be added when the factory-reset contract is
    // defined. For now, the central reset point performs a controlled reboot.
    ESP_LOGW(kTag, "completed action=restart");
    vTaskDelay(kLogFlushDelay);
    esp_restart();
}
