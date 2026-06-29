#include "iot154_battery.h"

#include <stdbool.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "iot154_sensor_config.h"

static const char *TAG = "iot154_battery";

static adc_oneshot_unit_handle_t s_adc_handle;
static adc_cali_handle_t s_cali_handle;
static bool s_adc_calibrated;

void iot154_battery_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = IOT154_BAT_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &s_adc_handle));

    adc_oneshot_chan_cfg_t channel_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle,
                                               IOT154_BAT_ADC_CHANNEL,
                                               &channel_config));

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = IOT154_BAT_ADC_UNIT,
        .chan = IOT154_BAT_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    esp_err_t err = adc_cali_create_scheme_curve_fitting(&cali_config, &s_cali_handle);
    s_adc_calibrated = err == ESP_OK;
    if (s_adc_calibrated) {
        ESP_LOGI(TAG, "battery ADC calibration enabled");
    } else {
        ESP_LOGW(TAG, "battery ADC calibration not available: %s", esp_err_to_name(err));
    }
}

uint8_t iot154_battery_percent_from_mv(uint16_t battery_mv)
{
    if (battery_mv <= IOT154_BAT_LOW_MV) {
        return 0;
    }
    if (battery_mv >= IOT154_BAT_FULL_MV) {
        return 100;
    }

    return (uint8_t)(((uint32_t)(battery_mv - IOT154_BAT_LOW_MV) * 100U) /
                     (IOT154_BAT_FULL_MV - IOT154_BAT_LOW_MV));
}

uint16_t iot154_battery_read_mv(void)
{
    int samples[IOT154_BAT_ADC_VALID_SAMPLES] = {0};
    int adc_mv = 0;

    for (uint8_t i = 0; i < IOT154_BAT_ADC_DISCARD_SAMPLES; ++i) {
        int discard = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(s_adc_handle, IOT154_BAT_ADC_CHANNEL, &discard));
    }

    for (uint8_t i = 0; i < IOT154_BAT_ADC_VALID_SAMPLES; ++i) {
        ESP_ERROR_CHECK(adc_oneshot_read(s_adc_handle, IOT154_BAT_ADC_CHANNEL, &samples[i]));
        if (i + 1 < IOT154_BAT_ADC_VALID_SAMPLES) {
            vTaskDelay(pdMS_TO_TICKS(IOT154_BAT_ADC_SAMPLE_DELAY_MS));
        }
    }

    for (uint8_t i = 1; i < IOT154_BAT_ADC_VALID_SAMPLES; ++i) {
        int value = samples[i];
        uint8_t j = i;
        while (j > 0 && samples[j - 1] > value) {
            samples[j] = samples[j - 1];
            --j;
        }
        samples[j] = value;
    }

    int center_sum = 0;
    const uint8_t center_start = (IOT154_BAT_ADC_VALID_SAMPLES - IOT154_BAT_ADC_CENTER_SAMPLES) / 2;
    for (uint8_t i = 0; i < IOT154_BAT_ADC_CENTER_SAMPLES; ++i) {
        center_sum += samples[center_start + i];
    }
    const int filtered_raw = (center_sum + (IOT154_BAT_ADC_CENTER_SAMPLES / 2)) /
                             IOT154_BAT_ADC_CENTER_SAMPLES;

    if (s_adc_calibrated) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_cali_handle, filtered_raw, &adc_mv));
    } else {
        adc_mv = (filtered_raw * 3300) / 4095;
    }

    const float measured_battery_mv = adc_mv * ((IOT154_BAT_R_TOP + IOT154_BAT_R_BOTTOM) / IOT154_BAT_R_BOTTOM);
    const uint16_t measured_rounded_mv = (uint16_t)(measured_battery_mv + 0.5f);
    const uint16_t rounded_mv =
        (uint16_t)(((uint32_t)measured_rounded_mv * IOT154_BAT_VOLTAGE_SCALE_NUM +
                    (IOT154_BAT_VOLTAGE_SCALE_DEN / 2U)) /
                   IOT154_BAT_VOLTAGE_SCALE_DEN);
    const uint8_t percent = iot154_battery_percent_from_mv(rounded_mv);

    ESP_LOGI(TAG,
             "BATTERY raw_min=%d raw_max=%d raw_center_avg=%d adc=%dmV measured=%umV battery=%umV percent=%u%%",
             samples[0],
             samples[IOT154_BAT_ADC_VALID_SAMPLES - 1],
             filtered_raw,
             adc_mv,
             measured_rounded_mv,
             rounded_mv,
             percent);

    if (rounded_mv >= IOT154_BAT_FULL_MV) {
        ESP_LOGI(TAG, "Battery: full (%u%%)", percent);
    } else if (rounded_mv <= IOT154_BAT_LOW_MV) {
        ESP_LOGW(TAG, "Battery: low (%u%%)", percent);
    }

    return rounded_mv;
}
