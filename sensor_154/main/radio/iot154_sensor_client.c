#include "iot154_sensor_client.h"

#include <string.h>

#include "esp_attr.h"
#include "esp_check.h"
#include "esp_ieee802154.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "iot154_packet.h"
#include "iot154_radio.h"
#include "iot154_sensor_config.h"

static const char *TAG = "iot154_client";
static const EventBits_t RX_DONE_BIT = BIT0;
static const EventBits_t TX_DONE_BIT = BIT1;
static const EventBits_t TX_FAILED_BIT = BIT2;

static EventGroupHandle_t s_events;
static uint8_t s_rx_frame[IOT154_MAX_FRAME_LEN + 1];
static uint8_t s_tx_frame[IOT154_MAX_FRAME_LEN + 1];
static uint16_t s_waiting_seq;
static int64_t s_tx_start_us;
static uint8_t s_mac_seq;
static uint8_t s_sensor_ext_addr[IOT154_EXT_ADDR_LEN];
static uint8_t s_central_ext_addr[IOT154_EXT_ADDR_LEN];

static void IRAM_ATTR on_rx_done(uint8_t *frame, esp_ieee802154_frame_info_t *frame_info)
{
    BaseType_t task_woken = pdFALSE;
    uint8_t len = frame[0];
    if (len <= IOT154_MAX_FRAME_LEN) {
        memcpy(s_rx_frame, frame, len + 1);
        xEventGroupSetBitsFromISR(s_events, RX_DONE_BIT, &task_woken);
    }
    esp_ieee802154_receive_handle_done(frame);
    portYIELD_FROM_ISR(task_woken);
}

static void IRAM_ATTR on_tx_done(const uint8_t *frame,
                                 const uint8_t *ack,
                                 esp_ieee802154_frame_info_t *ack_info)
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

esp_err_t iot154_sensor_client_init(const uint8_t *sensor_ext_addr)
{
    memcpy(s_sensor_ext_addr, sensor_ext_addr, IOT154_EXT_ADDR_LEN);
    s_events = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_events != NULL, ESP_ERR_NO_MEM, TAG, "create event group");
    ESP_RETURN_ON_ERROR(iot154_radio_init(IOT154_SENSOR_ADDR, false, on_rx_done, on_tx_done, on_tx_failed),
                        TAG,
                        "init radio");
    return esp_ieee802154_set_extended_address(s_sensor_ext_addr);
}

void iot154_sensor_client_set_central_ext_addr(const uint8_t *central_ext_addr)
{
    memcpy(s_central_ext_addr, central_ext_addr, IOT154_EXT_ADDR_LEN);
}

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

bool iot154_sensor_client_discover_central(uint16_t seq, uint8_t *central_ext_addr)
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
        if ((bits & RX_DONE_BIT) != 0 && received_discovery_response(central_ext_addr)) {
            iot154_sensor_client_set_central_ext_addr(central_ext_addr);
            ESP_LOGI(TAG, "PAIR central_ext=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                     central_ext_addr[0],
                     central_ext_addr[1],
                     central_ext_addr[2],
                     central_ext_addr[3],
                     central_ext_addr[4],
                     central_ext_addr[5],
                     central_ext_addr[6],
                     central_ext_addr[7]);
            return true;
        }
    }

    return false;
}

bool iot154_sensor_client_transmit_data_with_ack(uint16_t seq,
                                                 uint8_t event_type,
                                                 uint8_t value,
                                                 iot154_sensor_tx_result_t *result)
{
    result->attempts = 0;
    result->tx_start_us = 0;
    result->ack_received_us = -1;

    for (uint8_t attempt = 1; attempt <= IOT154_MAX_TX_ATTEMPTS; ++attempt) {
        if (attempt == 2) {
            vTaskDelay(pdMS_TO_TICKS(5));
        } else if (attempt == 3) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        xEventGroupClearBits(s_events, RX_DONE_BIT | TX_DONE_BIT | TX_FAILED_BIT);
        result->attempts = attempt;

        esp_err_t err = send_data(seq, event_type, value);
        result->tx_start_us = s_tx_start_us;
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
            result->ack_received_us = esp_timer_get_time();
            return true;
        }
    }

    return false;
}
