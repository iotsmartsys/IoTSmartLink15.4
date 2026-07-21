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

FactoryResetService::FactoryResetService(FactoryResetCleanup cleanup,
                                         void *cleanupContext)
    : requested_(false),
      cleanup_(cleanup),
      cleanupContext_(cleanupContext)
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
    ESP_LOGW(kTag, "begin persistence_preserved=no");

    if (cleanup_ == nullptr) {
        ESP_LOGE(kTag, "failed reason=cleanup_unavailable");
        requested_ = false;
        return;
    }
    const esp_err_t cleanupResult = cleanup_(cleanupContext_);
    if (cleanupResult != ESP_OK) {
        ESP_LOGE(kTag, "failed reason=persistence_cleanup result=%s",
                 esp_err_to_name(cleanupResult));
        requested_ = false;
        return;
    }
    ESP_LOGW(kTag, "completed action=restart");
    vTaskDelay(kLogFlushDelay);
    esp_restart();
}
