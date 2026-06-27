#include <inttypes.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_attr.h"
#include "esp_check.h"
#include "esp_ieee802154.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "iot154_packet.h"
#include "iot154_radio.h"

static const char *TAG = "sensor_154";
static const EventBits_t RX_DONE_BIT = BIT0;
static const EventBits_t TX_DONE_BIT = BIT1;
static const EventBits_t TX_FAILED_BIT = BIT2;

#if CONFIG_IDF_TARGET_ESP32C6
#define IOT154_SENSOR_GPIO GPIO_NUM_4
#else
#define IOT154_SENSOR_GPIO GPIO_NUM_14
#endif

#define IOT154_SKIP_DEEP_SLEEP 0
#define IOT154_SENSOR_HIGH 1
#define IOT154_SENSOR_LOW 0
#define IOT154_SENSOR_LOGIC_ACTIVE IOT154_SENSOR_HIGH

#define IOT154_GPIO_SAMPLES 5
#define IOT154_GPIO_ACTIVE_MIN_SAMPLES 3
#define IOT154_GPIO_SAMPLE_DELAY_MS 3
#define IOT154_GPIO_STABLE_READS 2
#define IOT154_GPIO_STABLE_MAX_READS 4
#define IOT154_GPIO_STABLE_DELAY_MS 8
#define IOT154_MAX_TX_ATTEMPTS 3
#define IOT154_MAX_STATE_UPDATES 4
#define IOT154_ACK_WAIT_MS 50
#define IOT154_STATS_WINDOW 20
#define IOT154_ISSP154_BASELINE_MS 3000
#define IOT154_DISCOVERY_WAIT_MS 120
#define IOT154_NVS_NAMESPACE "iot154"
#define IOT154_NVS_CENTRAL_KEY "central"
#define IOT154_BAT_ADC_CHANNEL ADC_CHANNEL_0
#define IOT154_BAT_ADC_UNIT ADC_UNIT_1
#define IOT154_BAT_R_TOP 470000.0f
#define IOT154_BAT_R_BOTTOM 220000.0f
#define IOT154_BAT_FULL_MV 4150
#define IOT154_BAT_LOW_MV 3300

#ifndef IOT154_SKIP_DEEP_SLEEP
#define IOT154_SKIP_DEEP_SLEEP 0
#endif

RTC_DATA_ATTR static uint16_t s_rtc_seq;

static EventGroupHandle_t s_events;
static uint8_t s_rx_frame[IOT154_MAX_FRAME_LEN + 1];
static uint8_t s_rx_len;
static uint8_t s_tx_frame[IOT154_MAX_FRAME_LEN + 1];
static uint16_t s_waiting_seq;
static int64_t s_tx_start_us;
static uint8_t s_mac_seq;
static uint8_t s_sensor_ext_addr[IOT154_EXT_ADDR_LEN];
static uint8_t s_central_ext_addr[IOT154_EXT_ADDR_LEN];
static adc_oneshot_unit_handle_t s_battery_adc_handle;
static adc_cali_handle_t s_battery_cali_handle;
static bool s_battery_adc_calibrated;

typedef struct {
    int64_t boot_us;
    int64_t gpio_sampled_us;
    int64_t radio_init_done_us;
    int64_t tx_start_us;
    int64_t ack_received_us;
    int64_t sleep_enter_us;
} sensor_metrics_t;

typedef struct {
    uint32_t count;
    uint32_t min_total_ms;
    uint32_t max_total_ms;
    uint32_t last20_total_ms[IOT154_STATS_WINDOW];
    uint8_t last20_pos;
    uint8_t last20_count;
} sensor_stats_t;

RTC_DATA_ATTR static sensor_stats_t s_rtc_stats;

/// @brief Copy ACK candidate and release the driver RX buffer.
static void IRAM_ATTR on_rx_done(uint8_t *frame, esp_ieee802154_frame_info_t *frame_info)
{
    BaseType_t task_woken = pdFALSE;
    uint8_t len = frame[0];
    if (len <= IOT154_MAX_FRAME_LEN) {
        memcpy(s_rx_frame, frame, len + 1);
        s_rx_len = len;
        xEventGroupSetBitsFromISR(s_events, RX_DONE_BIT, &task_woken);
    }
    esp_ieee802154_receive_handle_done(frame);
    portYIELD_FROM_ISR(task_woken);
}

static void IRAM_ATTR on_tx_done(const uint8_t *frame, const uint8_t *ack, esp_ieee802154_frame_info_t *ack_info)
{
    BaseType_t task_woken = pdFALSE;
    xEventGroupSetBitsFromISR(s_events, TX_DONE_BIT, &task_woken);
    if (ack != NULL) {
        esp_ieee802154_receive_handle_done(ack);
    }
    portYIELD_FROM_ISR(task_woken);
}

static void IRAM_ATTR on_tx_failed(const uint8_t *frame, esp_ieee802154_tx_error_t error)
{
    BaseType_t task_woken = pdFALSE;
    xEventGroupSetBitsFromISR(s_events, TX_FAILED_BIT, &task_woken);
    portYIELD_FROM_ISR(task_woken);
}

/// @brief Configure GPIO14 as pull-up input for the dry contact.
static void configure_sensor_gpio(void)
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

/// @brief Configure ADC1 channel 0 for the battery divider on GPIO1.
static void init_battery_adc(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = IOT154_BAT_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &s_battery_adc_handle));

    adc_oneshot_chan_cfg_t channel_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_battery_adc_handle,
                                               IOT154_BAT_ADC_CHANNEL,
                                               &channel_config));

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = IOT154_BAT_ADC_UNIT,
        .chan = IOT154_BAT_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    esp_err_t err = adc_cali_create_scheme_curve_fitting(&cali_config, &s_battery_cali_handle);
    s_battery_adc_calibrated = err == ESP_OK;
    if (s_battery_adc_calibrated) {
        ESP_LOGI(TAG, "battery ADC calibration enabled");
    } else {
        ESP_LOGW(TAG, "battery ADC calibration not available: %s", esp_err_to_name(err));
    }
}

/// @brief Convert battery millivolts to a linear 0-100% estimate.
static uint8_t battery_percent_from_mv(uint16_t battery_mv)
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

/// @brief Read battery voltage in millivolts through the resistor divider.
static uint16_t read_battery_mv(void)
{
    int raw = 0;
    int adc_mv = 0;

    ESP_ERROR_CHECK(adc_oneshot_read(s_battery_adc_handle, IOT154_BAT_ADC_CHANNEL, &raw));

    if (s_battery_adc_calibrated) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_battery_cali_handle, raw, &adc_mv));
    } else {
        adc_mv = (raw * 3300) / 4095;
    }

    const float battery_mv = adc_mv * ((IOT154_BAT_R_TOP + IOT154_BAT_R_BOTTOM) / IOT154_BAT_R_BOTTOM);
    uint16_t rounded_mv = (uint16_t)(battery_mv + 0.5f);
    const uint8_t percent = battery_percent_from_mv(rounded_mv);

    ESP_LOGI(TAG,
             "BATTERY raw=%d adc=%dmV battery=%umV percent=%u%%",
             raw,
             adc_mv,
             rounded_mv,
             percent);

    if (rounded_mv >= IOT154_BAT_FULL_MV) {
        ESP_LOGI(TAG, "Battery: full (%u%%)", percent);
    } else if (rounded_mv <= IOT154_BAT_LOW_MV) {
        ESP_LOGW(TAG, "Battery: low (%u%%)", percent);
    }

    return rounded_mv;
}

/// @brief Sample GPIO14 with a small debounce window and return one debounced GPIO level.
static uint8_t sample_sensor_gpio_level_once(void)
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

/// @brief Read the physical GPIO level until it is stable across consecutive debounce windows.
static uint8_t sample_sensor_gpio_level(void)
{
    uint8_t last_level = sample_sensor_gpio_level_once();
    uint8_t stable_reads = 1;

    for (uint8_t i = 1; i < IOT154_GPIO_STABLE_MAX_READS; ++i) {
        vTaskDelay(pdMS_TO_TICKS(IOT154_GPIO_STABLE_DELAY_MS));
        const uint8_t level = sample_sensor_gpio_level_once();
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

/// @brief Convert the sampled GPIO level to the logical value sent to the central.
static uint8_t sensor_data_value(uint8_t gpio_level)
{
    return gpio_level == IOT154_SENSOR_LOGIC_ACTIVE ? 1 : 0;
}

/// @brief Initialize NVS for storing the discovered central address.
static void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
        return;
    }
    ESP_ERROR_CHECK(err);
}

/// @brief Load central extended address from NVS.
static bool load_central_ext_addr(uint8_t *addr)
{
    nvs_handle_t nvs = 0;
    size_t len = IOT154_EXT_ADDR_LEN;
    esp_err_t err = nvs_open(IOT154_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return false;
    }

    err = nvs_get_blob(nvs, IOT154_NVS_CENTRAL_KEY, addr, &len);
    nvs_close(nvs);
    return err == ESP_OK && len == IOT154_EXT_ADDR_LEN;
}

/// @brief Save central extended address to NVS after discovery.
static void save_central_ext_addr(const uint8_t *addr)
{
    nvs_handle_t nvs = 0;
    ESP_ERROR_CHECK(nvs_open(IOT154_NVS_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_blob(nvs, IOT154_NVS_CENTRAL_KEY, addr, IOT154_EXT_ADDR_LEN));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
}

/// @brief Send one DATA packet with the supplied event value.
static esp_err_t send_data(uint16_t seq, uint8_t event_type, uint8_t value)
{
    iot154_packet_t packet = {
        .version = IOT154_VERSION,
        .msg_type = IOT154_MSG_DATA,
        .device_id = IOT154_SENSOR_DEVICE_ID,
        .seq = seq,
        .event_type = event_type,
        .value = value,
    };
    iot154_packet_finalize(&packet);
    iot154_build_ext_frame(s_tx_frame, s_sensor_ext_addr, s_central_ext_addr, s_mac_seq++, &packet);

    s_waiting_seq = seq;
    s_tx_start_us = esp_timer_get_time();

    esp_ieee802154_sleep();
    return esp_ieee802154_transmit(s_tx_frame, true);
}

/// @brief Broadcast a discovery request using the sensor extended address as source.
static esp_err_t send_discovery_request(uint16_t seq)
{
    iot154_packet_t packet = {
        .version = IOT154_VERSION,
        .msg_type = IOT154_MSG_DISCOVERY_REQ,
        .device_id = IOT154_SENSOR_DEVICE_ID,
        .seq = seq,
        .event_type = 0,
        .value = 0,
    };
    iot154_packet_finalize(&packet);
    iot154_build_broadcast_from_ext_frame(s_tx_frame, s_sensor_ext_addr, s_mac_seq++, &packet);

    s_waiting_seq = seq;
    s_tx_start_us = esp_timer_get_time();

    esp_ieee802154_sleep();
    return esp_ieee802154_transmit(s_tx_frame, true);
}

/// @brief Check whether the last received frame is a matching application ACK.
static bool received_matching_ack(void)
{
    iot154_frame_info_t mac = {0};
    iot154_packet_t packet = {0};

    if (!iot154_parse_frame_info(s_rx_frame, &mac, &packet)) {
        return false;
    }

    return mac.src_mode == IOT154_ADDR_MODE_EXT &&
           mac.dst_mode == IOT154_ADDR_MODE_EXT &&
           iot154_ext_addr_equal(mac.src_ext, s_central_ext_addr) &&
           iot154_ext_addr_equal(mac.dst_ext, s_sensor_ext_addr) &&
           packet.device_id == IOT154_SENSOR_DEVICE_ID &&
           packet.msg_type == IOT154_MSG_ACK &&
           packet.seq == s_waiting_seq &&
           packet.value == IOT154_ACK_STATUS_OK;
}

/// @brief Check for a discovery response and copy the central source extended address.
static bool received_discovery_response(uint8_t *central_ext_addr)
{
    iot154_frame_info_t mac = {0};
    iot154_packet_t packet = {0};

    if (!iot154_parse_frame_info(s_rx_frame, &mac, &packet)) {
        return false;
    }

    if (mac.src_mode == IOT154_ADDR_MODE_EXT &&
        mac.dst_mode == IOT154_ADDR_MODE_EXT &&
        iot154_ext_addr_equal(mac.dst_ext, s_sensor_ext_addr) &&
        packet.device_id == IOT154_SENSOR_DEVICE_ID &&
        packet.msg_type == IOT154_MSG_DISCOVERY_RESP &&
        packet.seq == s_waiting_seq &&
        packet.value == IOT154_ACK_STATUS_OK) {
        memcpy(central_ext_addr, mac.src_ext, IOT154_EXT_ADDR_LEN);
        return true;
    }

    return false;
}

/// @brief Discover central by broadcast and persist its extended address.
static bool discover_central(uint16_t seq)
{
    for (uint8_t attempt = 1; attempt <= IOT154_MAX_TX_ATTEMPTS; ++attempt) {
        xEventGroupClearBits(s_events, RX_DONE_BIT | TX_DONE_BIT | TX_FAILED_BIT);

        esp_err_t err = send_discovery_request(seq);
        if (err != ESP_OK) {
            continue;
        }

        EventBits_t bits = xEventGroupWaitBits(s_events,
                                               TX_DONE_BIT | TX_FAILED_BIT,
                                               pdTRUE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(100));
        if ((bits & TX_DONE_BIT) == 0 || (bits & TX_FAILED_BIT) != 0) {
            continue;
        }

        ESP_ERROR_CHECK(iot154_radio_start_rx());
        bits = xEventGroupWaitBits(s_events, RX_DONE_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(IOT154_DISCOVERY_WAIT_MS));
        if ((bits & RX_DONE_BIT) != 0 && received_discovery_response(s_central_ext_addr)) {
            save_central_ext_addr(s_central_ext_addr);
            ESP_LOGI(TAG, "PAIR central_ext=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                     s_central_ext_addr[0],
                     s_central_ext_addr[1],
                     s_central_ext_addr[2],
                     s_central_ext_addr[3],
                     s_central_ext_addr[4],
                     s_central_ext_addr[5],
                     s_central_ext_addr[6],
                     s_central_ext_addr[7]);
            return true;
        }
    }

    return false;
}

/// @brief Send the current logical value and wait for its application ACK.
static bool transmit_data_with_ack(uint16_t seq,
                                   uint8_t event_type,
                                   uint8_t value,
                                   uint8_t *attempts_used,
                                   sensor_metrics_t *metrics)
{
    *attempts_used = 0;
    metrics->ack_received_us = -1;

    for (uint8_t attempt = 1; attempt <= IOT154_MAX_TX_ATTEMPTS; ++attempt) {
        if (attempt == 2) {
            vTaskDelay(pdMS_TO_TICKS(5));
        } else if (attempt == 3) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        xEventGroupClearBits(s_events, RX_DONE_BIT | TX_DONE_BIT | TX_FAILED_BIT);
        *attempts_used = attempt;

        esp_err_t err = send_data(seq, event_type, value);
        metrics->tx_start_us = s_tx_start_us;
        if (err != ESP_OK) {
            continue;
        }

        EventBits_t bits = xEventGroupWaitBits(s_events,
                                               TX_DONE_BIT | TX_FAILED_BIT,
                                               pdTRUE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(100));
        if ((bits & TX_FAILED_BIT) != 0) {
            continue;
        }
        if ((bits & TX_DONE_BIT) == 0) {
            continue;
        }

        ESP_ERROR_CHECK(iot154_radio_start_rx());
        bits = xEventGroupWaitBits(s_events, RX_DONE_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(IOT154_ACK_WAIT_MS));
        if ((bits & RX_DONE_BIT) != 0 && received_matching_ack()) {
            metrics->ack_received_us = esp_timer_get_time();
            return true;
        }
    }

    return false;
}

/// @brief Convert a timestamp difference to whole milliseconds.
static uint32_t elapsed_ms(int64_t start_us, int64_t end_us)
{
    if (end_us < start_us) {
        return 0;
    }
    return (uint32_t)((end_us - start_us) / 1000);
}

/// @brief Update RTC statistics that survive deep sleep.
static uint32_t update_total_stats(uint32_t total_awake_ms)
{
    if (s_rtc_stats.count == 0 || total_awake_ms < s_rtc_stats.min_total_ms) {
        s_rtc_stats.min_total_ms = total_awake_ms;
    }
    if (s_rtc_stats.count == 0 || total_awake_ms > s_rtc_stats.max_total_ms) {
        s_rtc_stats.max_total_ms = total_awake_ms;
    }

    s_rtc_stats.last20_total_ms[s_rtc_stats.last20_pos] = total_awake_ms;
    s_rtc_stats.last20_pos = (uint8_t)((s_rtc_stats.last20_pos + 1) % IOT154_STATS_WINDOW);
    if (s_rtc_stats.last20_count < IOT154_STATS_WINDOW) {
        ++s_rtc_stats.last20_count;
    }
    ++s_rtc_stats.count;

    uint32_t sum = 0;
    for (uint8_t i = 0; i < s_rtc_stats.last20_count; ++i) {
        sum += s_rtc_stats.last20_total_ms[i];
    }
    return sum / s_rtc_stats.last20_count;
}

/// @brief Configure the next wake level opposite to the current stable state and enter deep sleep.
static void enter_deep_sleep(uint16_t seq,
                             uint8_t gpio_level,
                             uint8_t attempts,
                             bool delivered,
                             const sensor_metrics_t *metrics)
{
    const esp_sleep_ext1_wakeup_mode_t wake_mode =
        gpio_level != 0 ? ESP_EXT1_WAKEUP_ANY_LOW : ESP_EXT1_WAKEUP_ANY_HIGH;
    const uint32_t boot_to_gpio_ms = elapsed_ms(metrics->boot_us, metrics->gpio_sampled_us);
    const uint32_t gpio_to_radio_ms = elapsed_ms(metrics->gpio_sampled_us, metrics->radio_init_done_us);
    const uint32_t radio_to_tx_ms = elapsed_ms(metrics->radio_init_done_us, metrics->tx_start_us);
    const uint32_t tx_to_ack_ms = delivered ? elapsed_ms(metrics->tx_start_us, metrics->ack_received_us) : 0;
    const uint32_t total_awake_ms = elapsed_ms(metrics->boot_us, metrics->sleep_enter_us);
    const uint32_t avg20_total_ms = update_total_stats(total_awake_ms);
    const int32_t gain_percent =
        (int32_t)(((int64_t)IOT154_ISSP154_BASELINE_MS - total_awake_ms) * 100 / IOT154_ISSP154_BASELINE_MS);

    ESP_ERROR_CHECK(esp_ieee802154_sleep());
    ESP_ERROR_CHECK(esp_ieee802154_disable());
    configure_sensor_gpio();

    ESP_LOGI(TAG,
             "RESULT seq=%u gpio=%u attempts=%u ack=%s "
             "boot_to_gpio=%" PRIu32 "ms gpio_to_radio=%" PRIu32 "ms "
             "radio_to_tx=%" PRIu32 "ms tx_to_ack=%" PRIu32 "ms total_awake=%" PRIu32 "ms "
             "min_total=%" PRIu32 "ms max_total=%" PRIu32 "ms avg20_total=%" PRIu32 "ms "
             "ISSP154_baseline=%ums gain=%" PRId32 "%% next_wake=%s skip_deep_sleep=%s",
             seq,
             gpio_level,
             attempts,
             delivered ? "true" : "false",
             boot_to_gpio_ms,
             gpio_to_radio_ms,
             radio_to_tx_ms,
             tx_to_ack_ms,
             total_awake_ms,
             s_rtc_stats.min_total_ms,
             s_rtc_stats.max_total_ms,
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
    esp_deep_sleep_start();
#endif
}

void app_main(void)
{
    sensor_metrics_t metrics = {
        .boot_us = esp_timer_get_time(),
        .ack_received_us = -1,
    };

    configure_sensor_gpio();
    uint8_t gpio_level = sample_sensor_gpio_level();
    metrics.gpio_sampled_us = esp_timer_get_time();
    init_battery_adc();
    const uint16_t battery_mv = read_battery_mv();
    const uint8_t battery_percent = battery_percent_from_mv(battery_mv);

    init_nvs();
    ESP_ERROR_CHECK(esp_read_mac(s_sensor_ext_addr, ESP_MAC_IEEE802154));
    s_events = xEventGroupCreate();
    ESP_ERROR_CHECK(iot154_radio_init(IOT154_SENSOR_ADDR, false, on_rx_done, on_tx_done, on_tx_failed));
    ESP_ERROR_CHECK(esp_ieee802154_set_extended_address(s_sensor_ext_addr));
    metrics.radio_init_done_us = esp_timer_get_time();

    uint16_t seq = (uint16_t)(s_rtc_seq + 1);

    if (!load_central_ext_addr(s_central_ext_addr) && !discover_central(seq)) {
        s_rtc_seq = seq;
        metrics.tx_start_us = s_tx_start_us;
        metrics.sleep_enter_us = esp_timer_get_time();
        enter_deep_sleep(seq, gpio_level, IOT154_MAX_TX_ATTEMPTS, false, &metrics);
    }

    bool delivered = false;
    uint8_t attempts_used = 0;
    for (uint8_t update = 0; update < IOT154_MAX_STATE_UPDATES; ++update) {
        const uint8_t value = sensor_data_value(gpio_level);
        delivered = transmit_data_with_ack(seq, IOT154_EVENT_DOOR, value, &attempts_used, &metrics);

        const uint8_t current_gpio_level = sample_sensor_gpio_level();
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
    delivered = transmit_data_with_ack(seq, IOT154_EVENT_BATTERY, battery_percent, &attempts_used, &metrics);

    s_rtc_seq = seq;

    metrics.sleep_enter_us = esp_timer_get_time();
    enter_deep_sleep(seq, gpio_level, attempts_used, delivered, &metrics);
}
