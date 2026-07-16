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
#define IOT154_RELAY_BUTTON_GPIO GPIO_NUM_9
#define IOT154_RELAY_ENDPOINT_ID 1
#define IOT154_RELAY_BUTTON_DEBOUNCE_MS 50
#define IOT154_SENSOR_DEVICE_ID 0x15400001

static const char *TAG = "iot154_switch";
static bool s_relay_on;
static bool s_relay_report_pending;
static bool s_relay_report_state;
static uint32_t s_relay_change_generation;
static bool s_button_stable_pressed;
static bool s_button_last_sample_pressed;
static TickType_t s_button_last_change_tick;

static bool discover_and_save_central(uint16_t seq, uint8_t *central_ext_addr)
{
    ESP_LOGI(TAG, "Searching for coordinator");
    if (!iot154_sensor_client_discover_central(seq, central_ext_addr))
    {
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
    s_relay_on = false;
    gpio_set_level(IOT154_RELAY_GPIO, 0);
}

static bool relay_button_is_pressed(void)
{
    return gpio_get_level(IOT154_RELAY_BUTTON_GPIO) == 0;
}

static void relay_button_configure(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = BIT64(IOT154_RELAY_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));

    s_button_stable_pressed = relay_button_is_pressed();
    s_button_last_sample_pressed = s_button_stable_pressed;
    s_button_last_change_tick = xTaskGetTickCount();
}

static void relay_mark_state_change(bool relay_on)
{
    s_relay_on = relay_on;
    s_relay_report_state = s_relay_on;
    s_relay_report_pending = true;
    ++s_relay_change_generation;
    ESP_LOGI(TAG, "Relay GPIO%d changed to %s", IOT154_RELAY_GPIO, s_relay_on ? "ON" : "OFF");
}

static bool relay_set(bool relay_on)
{
    if (s_relay_on == relay_on)
    {
        return false;
    }

    ESP_ERROR_CHECK(gpio_set_level(IOT154_RELAY_GPIO, relay_on ? 1 : 0));
    relay_mark_state_change(relay_on);
    return true;
}

static void relay_button_poll(void)
{
    const bool pressed = relay_button_is_pressed();
    const TickType_t now = xTaskGetTickCount();

    if (pressed != s_button_last_sample_pressed)
    {
        s_button_last_sample_pressed = pressed;
        s_button_last_change_tick = now;
        return;
    }

    if (pressed == s_button_stable_pressed ||
        now - s_button_last_change_tick < pdMS_TO_TICKS(IOT154_RELAY_BUTTON_DEBOUNCE_MS))
    {
        return;
    }

    s_button_stable_pressed = pressed;
    if (s_button_stable_pressed)
    {
        ESP_LOGI(TAG, "Relay GPIO%d physical toggle from GPIO%d", IOT154_RELAY_GPIO, IOT154_RELAY_BUTTON_GPIO);
        (void)relay_set(!s_relay_on);
    }
}

static uint8_t relay_command_callback(uint8_t endpoint_id, uint8_t event_type, uint8_t value)
{
    if (endpoint_id != IOT154_RELAY_ENDPOINT_ID || event_type != IOT154_EVENT_POWER)
    {
        return IOT154_ACK_STATUS_UNSUPPORTED;
    }

    if (value == IOT154_VALUE_OFF)
    {
        (void)relay_set(false);
        return IOT154_ACK_STATUS_OK;
    }
    if (value == IOT154_VALUE_ON)
    {
        (void)relay_set(true);
        return IOT154_ACK_STATUS_OK;
    }
    if (value == IOT154_VALUE_TOGGLE)
    {
        (void)relay_set(!s_relay_on);
        return IOT154_ACK_STATUS_OK;
    }

    return IOT154_ACK_STATUS_INVALID;
}

static bool report_relay_state(uint16_t *seq, bool *paired, uint8_t *central_ext_addr, bool relay_on)
{
    const uint8_t value = relay_on ? IOT154_VALUE_ON : IOT154_VALUE_OFF;

    if (!*paired)
    {
        *paired = discover_and_save_central(*seq, central_ext_addr);
        ++(*seq);
    }

    if (!*paired)
    {
        return false;
    }

    iot154_sensor_tx_result_t tx_result = {0};
    if (iot154_sensor_client_transmit_data_with_ack(*seq, IOT154_RELAY_ENDPOINT_ID, IOT154_EVENT_POWER, value, &tx_result))
    {
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

static void report_pending_relay_state(uint16_t *seq, bool *paired, uint8_t *central_ext_addr)
{
    if (!s_relay_report_pending)
    {
        return;
    }

    const bool report_state = s_relay_report_state;
    const uint32_t report_generation = s_relay_change_generation;
    if (report_relay_state(seq, paired, central_ext_addr, report_state) &&
        s_relay_change_generation == report_generation)
    {
        s_relay_report_pending = false;
    }
}

extern "C" void app_main()
{
    relay_configure();
    relay_button_configure();
    iot154_storage_init();

    uint8_t switch_ext_addr[IOT154_EXT_ADDR_LEN];
    uint8_t central_ext_addr[IOT154_EXT_ADDR_LEN];
    ESP_ERROR_CHECK(esp_read_mac(switch_ext_addr, ESP_MAC_IEEE802154));
    ESP_ERROR_CHECK(iot154_sensor_client_init(switch_ext_addr, IOT154_SENSOR_DEVICE_ID));
    iot154_sensor_client_set_command_callback(relay_command_callback);

    uint16_t seq = 1;
    bool paired = iot154_storage_load_central_ext_addr(central_ext_addr);
    if (paired)
    {
        iot154_sensor_client_set_central_ext_addr(central_ext_addr);
    }

    while (true)
    {
        (void)iot154_sensor_client_process_pending_command(100);
        relay_button_poll();
        report_pending_relay_state(&seq, &paired, central_ext_addr);
    }
}
