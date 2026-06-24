#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define BAT_ADC_CHANNEL   ADC_CHANNEL_0   // GPIO1 no ESP32-H2
#define BAT_ADC_UNIT      ADC_UNIT_1

#define R_TOP             470000.0f       // B+ -> GPIO1
#define R_BOTTOM          220000.0f       // GPIO1 -> GND

#define BLINK_GPIO        13
#define BLINK_PERIOD_MS   1000

static const char *TAG = "BATTERY";

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle = NULL;
static bool calibrated = false;

static void battery_adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = BAT_ADC_UNIT,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t channel_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(
        adc_handle,
        BAT_ADC_CHANNEL,
        &channel_config
    ));

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BAT_ADC_UNIT,
        .chan = BAT_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    if (adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle) == ESP_OK) {
        calibrated = true;
        ESP_LOGI(TAG, "ADC calibration enabled");
    } else {
        calibrated = false;
        ESP_LOGW(TAG, "ADC calibration not available");
    }
}

static float battery_read_voltage(void)
{
    int raw = 0;
    int adc_mv = 0;

    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, BAT_ADC_CHANNEL, &raw));

    if (calibrated) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw, &adc_mv));
    } else {
        adc_mv = (raw * 3300) / 4095;
    }

    float adc_voltage = adc_mv / 1000.0f;

    float battery_voltage =
        adc_voltage * ((R_TOP + R_BOTTOM) / R_BOTTOM);

    ESP_LOGI(TAG, "ADC raw=%d, ADC=%.3fV, Battery=%.3fV",
             raw, adc_voltage, battery_voltage);

    return battery_voltage;
}

static void blink_task(void *arg)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BLINK_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    bool state = false;
    while (1) {
        state = !state;
        gpio_set_level(BLINK_GPIO, state);
        vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD_MS / 2));
    }
}

void app_main(void)
{
    battery_adc_init();

    xTaskCreate(blink_task, "blink", 2048, NULL, 5, NULL);

    while (1) {
        float vbat = battery_read_voltage();

        if (vbat >= 4.15f) {
            ESP_LOGI(TAG, "Battery: full");
        } else if (vbat <= 3.30f) {
            ESP_LOGW(TAG, "Battery: low");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}