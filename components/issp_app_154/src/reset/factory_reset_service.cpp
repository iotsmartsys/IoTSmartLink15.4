#include "reset/factory_reset_service.hpp"

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
      cleanupContext_(cleanupContext),
      arbiter_{}
{
}

FactoryResetService::FactoryResetService(FactoryResetCleanup cleanup,
                                         void *cleanupContext,
                                         const FactoryResetArbiter &arbiter)
    : requested_(false),
      cleanup_(cleanup),
      cleanupContext_(cleanupContext),
      arbiter_(arbiter)
{
}

FactoryResetRequestResult FactoryResetService::requestFactoryReset()
{
    if (requested_)
    {
        return FactoryResetRequestResult::Accepted;
    }

    // First accepted transition wins. Losing to deep sleep is diagnosed and
    // does not consume the hold in course: the monitor may present the request
    // again if the transition is released by a wakeup-source failure.
    if (arbiter_.acquire != nullptr && !arbiter_.acquire(arbiter_.context))
    {
        ESP_LOGW(kTag, "rejected reason=deep_sleep_transition_held");
        return FactoryResetRequestResult::Rejected;
    }

    requested_ = true;
    ESP_LOGW(kTag, "requested");
    ESP_LOGW(kTag, "begin persistence_preserved=no");

    if (cleanup_ == nullptr) {
        ESP_LOGE(kTag, "failed reason=cleanup_unavailable");
        requested_ = false;
        if (arbiter_.release != nullptr)
        {
            arbiter_.release(arbiter_.context);
        }
        return FactoryResetRequestResult::Accepted;
    }
    const esp_err_t cleanupResult = cleanup_(cleanupContext_);
    if (cleanupResult != ESP_OK) {
        ESP_LOGE(kTag, "failed reason=persistence_cleanup result=%s",
                 esp_err_to_name(cleanupResult));
        requested_ = false;
        if (arbiter_.release != nullptr)
        {
            arbiter_.release(arbiter_.context);
        }
        return FactoryResetRequestResult::Accepted;
    }
    ESP_LOGW(kTag, "completed action=restart");
    vTaskDelay(kLogFlushDelay);
    esp_restart();
    return FactoryResetRequestResult::Accepted;
}
