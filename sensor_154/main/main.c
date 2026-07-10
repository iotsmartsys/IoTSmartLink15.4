#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "iot154_packet.h"
#include "iot154_sensor_client.h"
#include "iot154_storage.h"

#define IOT154_RELAY_GPIO GPIO_NUM_13
#define IOT154_RELAY_BLINK_MS 15000

static const char *TAG = "iot154_switch";

static bool discover_and_save_central(uint16_t seq, uint8_t *central_ext_addr)
{
    ESP_LOGI(TAG, "Searching for coordinator");
    if (!iot154_sensor_client_discover_central(seq, central_ext_addr)) {
        ESP_LOGW(TAG, "Coordinator discovery failed");
        return false;
    }

    iot154_storage_save_central_ext_addr(central_ext_addr);
    iot154_storage_reset_send_failures();
    return true;
}

static void relay_configure(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = BIT64(IOT154_RELAY_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    gpio_set_level(IOT154_RELAY_GPIO, 0);
}

static bool report_relay_state(uint16_t *seq, bool *paired, uint8_t *central_ext_addr, bool relay_on)
{
    const uint8_t value = relay_on ? IOT154_VALUE_ON : IOT154_VALUE_OFF;

    if (!*paired) {
        *paired = discover_and_save_central(*seq, central_ext_addr);
        ++(*seq);
    }

    if (!*paired) {
        return false;
    }

    iot154_sensor_tx_result_t tx_result = {0};
    if (iot154_sensor_client_transmit_data_with_ack(*seq, IOT154_EVENT_POWER, value, &tx_result)) {
        iot154_storage_reset_send_failures();
        ESP_LOGI(TAG, "Reported relay GPIO%d %s", IOT154_RELAY_GPIO, relay_on ? "ON" : "OFF");
        ++(*seq);
        return true;
    }

    const uint8_t failures = iot154_storage_record_send_failure();
    ESP_LOGW(TAG, "Relay state report failed; failures=%u", failures);
    ++(*seq);
    return false;
}

void app_main(void)
{
    relay_configure();
    iot154_storage_init();

    uint8_t switch_ext_addr[IOT154_EXT_ADDR_LEN];
    uint8_t central_ext_addr[IOT154_EXT_ADDR_LEN];
    ESP_ERROR_CHECK(esp_read_mac(switch_ext_addr, ESP_MAC_IEEE802154));
    ESP_ERROR_CHECK(iot154_sensor_client_init(switch_ext_addr));

    uint16_t seq = 1;
    bool paired = iot154_storage_load_central_ext_addr(central_ext_addr);
    if (paired) {
        iot154_sensor_client_set_central_ext_addr(central_ext_addr);
    }

    bool relay_on = false;
    while (true) {
        relay_on = !relay_on;
        ESP_ERROR_CHECK(gpio_set_level(IOT154_RELAY_GPIO, relay_on ? 1 : 0));
        ESP_LOGI(TAG, "Relay GPIO%d changed to %s", IOT154_RELAY_GPIO, relay_on ? "ON" : "OFF");
        (void)report_relay_state(&seq, &paired, central_ext_addr, relay_on);
        vTaskDelay(pdMS_TO_TICKS(IOT154_RELAY_BLINK_MS));
    }
}
