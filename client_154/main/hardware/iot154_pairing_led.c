#include "iot154_pairing_led.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "iot154_sensor_config.h"

static TaskHandle_t s_blink_task;

static void blink_task(void *arg)
{
    bool led_on = false;

    while (true) {
        led_on = !led_on;
        gpio_set_level(IOT154_PAIRING_LED_GPIO, led_on ? 1 : 0);
        vTaskDelay(pdMS_TO_TICKS(IOT154_PAIRING_LED_BLINK_MS));
    }
}

void iot154_pairing_led_configure(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = BIT64(IOT154_PAIRING_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    gpio_set_level(IOT154_PAIRING_LED_GPIO, 0);
}

void iot154_pairing_led_start_blink(void)
{
    iot154_pairing_led_configure();
    if (s_blink_task != NULL) {
        return;
    }

    ESP_ERROR_CHECK(xTaskCreate(blink_task, "pair_led", 2048, NULL, tskIDLE_PRIORITY + 1, &s_blink_task) == pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
}

void iot154_pairing_led_stop(void)
{
    if (s_blink_task != NULL) {
        vTaskDelete(s_blink_task);
        s_blink_task = NULL;
    }
    gpio_set_level(IOT154_PAIRING_LED_GPIO, 0);
}
