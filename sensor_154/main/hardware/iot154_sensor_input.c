#include "iot154_sensor_input.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "iot154_sensor_config.h"

void iot154_sensor_input_configure(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = BIT64(IOT154_SENSOR_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
}

static uint8_t sample_level_once(void)
{
    uint8_t low_count = 0;

    for (uint8_t i = 0; i < IOT154_GPIO_SAMPLES; ++i) {
        if (gpio_get_level(IOT154_SENSOR_GPIO) == 0) {
            ++low_count;
        }
        if (i + 1 < IOT154_GPIO_SAMPLES) {
            vTaskDelay(pdMS_TO_TICKS(IOT154_GPIO_SAMPLE_DELAY_MS));
        }
    }

    return low_count >= IOT154_GPIO_ACTIVE_MIN_SAMPLES ? IOT154_SENSOR_LOW : IOT154_SENSOR_HIGH;
}

uint8_t iot154_sensor_input_sample_level(void)
{
    uint8_t last_level = sample_level_once();
    uint8_t stable_reads = 1;

    for (uint8_t i = 1; i < IOT154_GPIO_STABLE_MAX_READS; ++i) {
        vTaskDelay(pdMS_TO_TICKS(IOT154_GPIO_STABLE_DELAY_MS));
        const uint8_t level = sample_level_once();
        if (level == last_level) {
            ++stable_reads;
            if (stable_reads >= IOT154_GPIO_STABLE_READS) {
                break;
            }
        } else {
            last_level = level;
            stable_reads = 1;
        }
    }

    return last_level;
}

uint8_t iot154_sensor_input_data_value(uint8_t gpio_level)
{
    return gpio_level == IOT154_SENSOR_LOGIC_ACTIVE ? 1 : 0;
}
