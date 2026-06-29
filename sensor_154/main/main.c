#include "esp_attr.h"
#include "esp_check.h"
#include "esp_mac.h"
#include "esp_timer.h"

#include "iot154_battery.h"
#include "iot154_metrics.h"
#include "iot154_packet.h"
#include "iot154_power.h"
#include "iot154_sensor_client.h"
#include "iot154_sensor_config.h"
#include "iot154_sensor_input.h"
#include "iot154_storage.h"

RTC_DATA_ATTR static uint16_t s_rtc_seq;

static void apply_tx_result(const iot154_sensor_tx_result_t *tx_result, iot154_sensor_metrics_t *metrics)
{
    metrics->tx_start_us = tx_result->tx_start_us;
    metrics->ack_received_us = tx_result->ack_received_us;
}

void app_main(void)
{
    iot154_sensor_metrics_t metrics = {
        .boot_us = esp_timer_get_time(),
        .ack_received_us = -1,
    };

    iot154_sensor_input_configure();
    uint8_t gpio_level = iot154_sensor_input_sample_level();
    metrics.gpio_sampled_us = esp_timer_get_time();

    iot154_battery_init();
    const uint16_t battery_mv = iot154_battery_read_mv();
    const uint8_t battery_percent = iot154_battery_percent_from_mv(battery_mv);

    iot154_storage_init();

    uint8_t sensor_ext_addr[IOT154_EXT_ADDR_LEN];
    uint8_t central_ext_addr[IOT154_EXT_ADDR_LEN];
    ESP_ERROR_CHECK(esp_read_mac(sensor_ext_addr, ESP_MAC_IEEE802154));
    ESP_ERROR_CHECK(iot154_sensor_client_init(sensor_ext_addr));
    metrics.radio_init_done_us = esp_timer_get_time();

    uint16_t seq = (uint16_t)(s_rtc_seq + 1);

    if (iot154_storage_load_central_ext_addr(central_ext_addr)) {
        iot154_sensor_client_set_central_ext_addr(central_ext_addr);
    } else if (iot154_sensor_client_discover_central(seq, central_ext_addr)) {
        iot154_storage_save_central_ext_addr(central_ext_addr);
    } else {
        s_rtc_seq = seq;
        metrics.sleep_enter_us = esp_timer_get_time();
        iot154_power_enter_deep_sleep(seq, gpio_level, IOT154_MAX_TX_ATTEMPTS, false, &metrics);
    }

    bool delivered = false;
    iot154_sensor_tx_result_t tx_result = {0};
    for (uint8_t update = 0; update < IOT154_MAX_STATE_UPDATES; ++update) {
        const uint8_t value = iot154_sensor_input_data_value(gpio_level);
        delivered = iot154_sensor_client_transmit_data_with_ack(seq, IOT154_EVENT_DOOR, value, &tx_result);
        apply_tx_result(&tx_result, &metrics);

        const uint8_t current_gpio_level = iot154_sensor_input_sample_level();
        if (current_gpio_level == gpio_level) {
            break;
        }
        if (update + 1 >= IOT154_MAX_STATE_UPDATES) {
            break;
        }

        gpio_level = current_gpio_level;
        seq = (uint16_t)(seq + 1);
    }

    seq = (uint16_t)(seq + 1);
    delivered = iot154_sensor_client_transmit_data_with_ack(seq, IOT154_EVENT_BATTERY, battery_percent, &tx_result);
    apply_tx_result(&tx_result, &metrics);

    s_rtc_seq = seq;

    metrics.sleep_enter_us = esp_timer_get_time();
    iot154_power_enter_deep_sleep(seq, gpio_level, tx_result.attempts, delivered, &metrics);
}
