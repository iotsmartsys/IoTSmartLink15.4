#include "iot154_power.h"

#include <inttypes.h>

#include "esp_check.h"
#include "esp_ieee802154.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "iot154_boot_button.h"
#include "iot154_sensor_config.h"
#include "iot154_sensor_input.h"

static const char *TAG = "iot154_power";

void iot154_power_enter_deep_sleep(uint16_t seq,
                                   uint8_t gpio_level,
                                   uint8_t attempts,
                                   bool delivered,
                                   const iot154_sensor_metrics_t *metrics)
{
    const esp_sleep_ext1_wakeup_mode_t wake_mode =
        gpio_level != 0 ? ESP_EXT1_WAKEUP_ANY_LOW : ESP_EXT1_WAKEUP_ANY_HIGH;
    const uint32_t boot_to_gpio_ms = iot154_metrics_elapsed_ms(metrics->boot_us, metrics->gpio_sampled_us);
    const uint32_t gpio_to_radio_ms = iot154_metrics_elapsed_ms(metrics->gpio_sampled_us,
                                                                metrics->radio_init_done_us);
    const uint32_t radio_to_tx_ms = iot154_metrics_elapsed_ms(metrics->radio_init_done_us, metrics->tx_start_us);
    const uint32_t tx_to_ack_ms = delivered ? iot154_metrics_elapsed_ms(metrics->tx_start_us,
                                                                        metrics->ack_received_us)
                                            : 0;
    const uint32_t total_awake_ms = iot154_metrics_elapsed_ms(metrics->boot_us, metrics->sleep_enter_us);
    const uint32_t avg20_total_ms = iot154_metrics_update_total_stats(total_awake_ms);
    const int32_t gain_percent =
        (int32_t)(((int64_t)IOT154_ISSP154_BASELINE_MS - total_awake_ms) * 100 / IOT154_ISSP154_BASELINE_MS);

    ESP_ERROR_CHECK(esp_ieee802154_sleep());
    ESP_ERROR_CHECK(esp_ieee802154_disable());
    iot154_sensor_input_configure();

    ESP_LOGI(TAG,
             "RESULT seq=%u gpio=%u attempts=%u ack=%s "
             "boot_to_gpio=%" PRIu32 "ms gpio_to_radio=%" PRIu32 "ms "
             "radio_to_tx=%" PRIu32 "ms tx_to_ack=%" PRIu32 "ms total_awake=%" PRIu32 "ms "
             "min_total=%" PRIu32 "ms max_total=%" PRIu32 "ms avg20_total=%" PRIu32 "ms "
             "ISSP154_baseline=%ums gain=%" PRId32 "%% next_wake_sensor=%s next_wake_boot=LOW skip_deep_sleep=%s",
             seq,
             gpio_level,
             attempts,
             delivered ? "true" : "false",
             boot_to_gpio_ms,
             gpio_to_radio_ms,
             radio_to_tx_ms,
             tx_to_ack_ms,
             total_awake_ms,
             iot154_metrics_min_total_ms(),
             iot154_metrics_max_total_ms(),
             avg20_total_ms,
             IOT154_ISSP154_BASELINE_MS,
             gain_percent,
             wake_mode == ESP_EXT1_WAKEUP_ANY_HIGH ? "HIGH" : "LOW",
             IOT154_SKIP_DEEP_SLEEP ? "true" : "false");

#if IOT154_SKIP_DEEP_SLEEP
    ESP_LOGW(TAG, "IOT154_SKIP_DEEP_SLEEP enabled; staying awake");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#else
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(BIT64(IOT154_SENSOR_GPIO), wake_mode));
    iot154_boot_button_configure();
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(BIT64(IOT154_BOOT_BUTTON_GPIO), ESP_EXT1_WAKEUP_ANY_LOW));
    esp_deep_sleep_start();
#endif
}
