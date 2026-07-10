#include "iot154_boot_button.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "iot154_sensor_config.h"

void iot154_boot_button_configure(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = BIT64(IOT154_BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
}

bool iot154_boot_button_is_pressed(void)
{
    uint8_t low_count = 0;

    for (uint8_t i = 0; i < IOT154_BOOT_BUTTON_SAMPLES; ++i) {
        if (gpio_get_level(IOT154_BOOT_BUTTON_GPIO) == 0) {
            ++low_count;
        }
        if (i + 1 < IOT154_BOOT_BUTTON_SAMPLES) {
            vTaskDelay(pdMS_TO_TICKS(IOT154_BOOT_BUTTON_SAMPLE_DELAY_MS));
        }
    }

    return low_count >= IOT154_BOOT_BUTTON_ACTIVE_MIN_SAMPLES;
}
