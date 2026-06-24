#include <inttypes.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_ieee802154.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"

#include "iot154_packet.h"
#include "iot154_radio.h"

static const char *TAG = "central_154";
static const EventBits_t RX_DONE_BIT = BIT0;
static const EventBits_t TX_DONE_BIT = BIT1;
static const EventBits_t TX_FAILED_BIT = BIT2;

static EventGroupHandle_t s_events;
static uint8_t s_rx_frame[IOT154_MAX_FRAME_LEN + 1];
static uint8_t s_rx_len;
static esp_ieee802154_frame_info_t s_rx_info;
static uint8_t s_ack_frame[IOT154_MAX_FRAME_LEN + 1];
static uint8_t s_mac_seq;
static uint16_t s_ack_tx_seq;
static esp_ieee802154_tx_error_t s_ack_tx_error;
static uint8_t s_central_ext_addr[IOT154_EXT_ADDR_LEN];
static const char *s_tx_type = "ACK";

typedef struct {
    uint32_t device_id;
    uint16_t last_seq;
    bool valid;
} device_seq_state_t;

static device_seq_state_t s_devices[8];

/// @brief Initialize NVS before RF calibration data is loaded by the PHY.
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

/// @brief Copy a received frame out of the driver buffer and release it.
static void IRAM_ATTR on_rx_done(uint8_t *frame, esp_ieee802154_frame_info_t *frame_info)
{
    BaseType_t task_woken = pdFALSE;
    uint8_t len = frame[0];
    if (len <= IOT154_MAX_FRAME_LEN) {
        memcpy(s_rx_frame, frame, len + 1);
        s_rx_len = len;
        s_rx_info = *frame_info;
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
    s_ack_tx_error = error;
    xEventGroupSetBitsFromISR(s_events, TX_FAILED_BIT, &task_woken);
    portYIELD_FROM_ISR(task_woken);
}

/// @brief Format an extended address for logs.
static void format_ext_addr(const uint8_t *addr, char *out, size_t out_len)
{
    snprintf(out,
             out_len,
             "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
}

/// @brief Send a protocol ACK packet carrying the received sequence number.
static void send_ack(uint32_t device_id, uint16_t seq, const uint8_t *dst_ext_addr)
{
    iot154_packet_t ack = {
        .version = IOT154_VERSION,
        .msg_type = IOT154_MSG_ACK,
        .device_id = device_id,
        .seq = seq,
        .event_type = 0,
        .value = IOT154_ACK_STATUS_OK,
    };
    iot154_packet_finalize(&ack);
    iot154_build_ext_frame(s_ack_frame, s_central_ext_addr, dst_ext_addr, s_mac_seq++, &ack);

    s_ack_tx_seq = seq;
    s_tx_type = "ACK";
    esp_ieee802154_sleep();
    esp_err_t err = esp_ieee802154_transmit(s_ack_frame, false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ACK TX failed seq=%u start_err=%s", seq, esp_err_to_name(err));
        iot154_radio_start_rx();
    }
}

/// @brief Reply to a sensor discovery request; the source MAC in this frame is the central identity.
static void send_discovery_response(uint32_t device_id, uint16_t seq, const uint8_t *dst_ext_addr)
{
    iot154_packet_t response = {
        .version = IOT154_VERSION,
        .msg_type = IOT154_MSG_DISCOVERY_RESP,
        .device_id = device_id,
        .seq = seq,
        .event_type = 0,
        .value = IOT154_ACK_STATUS_OK,
    };
    iot154_packet_finalize(&response);
    iot154_build_ext_frame(s_ack_frame, s_central_ext_addr, dst_ext_addr, s_mac_seq++, &response);

    s_ack_tx_seq = seq;
    s_tx_type = "DISCOVERY_RESP";
    esp_ieee802154_sleep();
    esp_err_t err = esp_ieee802154_transmit(s_ack_frame, false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DISCOVERY_RESP TX failed seq=%u start_err=%s", seq, esp_err_to_name(err));
        iot154_radio_start_rx();
    }
}

/// @brief Return true when this device_id + seq was already processed.
static bool is_duplicate(uint32_t device_id, uint16_t seq)
{
    device_seq_state_t *free_slot = NULL;

    for (size_t i = 0; i < sizeof(s_devices) / sizeof(s_devices[0]); ++i) {
        if (s_devices[i].valid && s_devices[i].device_id == device_id) {
            if (s_devices[i].last_seq == seq) {
                return true;
            }
            s_devices[i].last_seq = seq;
            return false;
        }
        if (!s_devices[i].valid && free_slot == NULL) {
            free_slot = &s_devices[i];
        }
    }

    if (free_slot == NULL) {
        free_slot = &s_devices[0];
    }

    free_slot->device_id = device_id;
    free_slot->last_seq = seq;
    free_slot->valid = true;
    return false;
}

void app_main(void)
{
    char central_mac_text[3 * IOT154_EXT_ADDR_LEN] = {0};

    init_nvs();
    ESP_ERROR_CHECK(esp_read_mac(s_central_ext_addr, ESP_MAC_IEEE802154));
    s_events = xEventGroupCreate();
    ESP_ERROR_CHECK(iot154_radio_init(IOT154_CENTRAL_ADDR, true, on_rx_done, on_tx_done, on_tx_failed));
    ESP_ERROR_CHECK(esp_ieee802154_set_extended_address(s_central_ext_addr));
    ESP_ERROR_CHECK(iot154_radio_start_rx());

    format_ext_addr(s_central_ext_addr, central_mac_text, sizeof(central_mac_text));
    ESP_LOGI(TAG,
             "central RX channel=%d pan=0x%04x short=0x%04x ext=%s",
             IOT154_CHANNEL,
             IOT154_PAN_ID,
             IOT154_CENTRAL_ADDR,
             central_mac_text);

    while (true) {
        EventBits_t bits = xEventGroupWaitBits(s_events,
                                               RX_DONE_BIT | TX_DONE_BIT | TX_FAILED_BIT,
                                               pdTRUE,
                                               pdFALSE,
                                               portMAX_DELAY);

        if ((bits & RX_DONE_BIT) != 0) {
            uint8_t frame[IOT154_MAX_FRAME_LEN + 1];
            memcpy(frame, s_rx_frame, s_rx_len + 1);

            iot154_frame_info_t mac = {0};
            iot154_packet_t packet = {0};
            if (!iot154_parse_frame_info(frame, &mac, &packet)) {
                ESP_LOGW(TAG, "ignored frame: invalid packet");
                continue;
            }

            if (packet.msg_type == IOT154_MSG_DISCOVERY_REQ &&
                mac.dst_mode == IOT154_ADDR_MODE_SHORT &&
                mac.dst_broadcast &&
                mac.src_mode == IOT154_ADDR_MODE_EXT) {
                char sensor_mac_text[3 * IOT154_EXT_ADDR_LEN] = {0};
                format_ext_addr(mac.src_ext, sensor_mac_text, sizeof(sensor_mac_text));
                ESP_LOGI(TAG, "DISCOVERY_REQ dev=0x%08" PRIx32 " seq=%u sensor=%s", packet.device_id, packet.seq, sensor_mac_text);
                send_discovery_response(packet.device_id, packet.seq, mac.src_ext);
            } else if (packet.msg_type == IOT154_MSG_DATA &&
                       mac.dst_mode == IOT154_ADDR_MODE_EXT &&
                       iot154_ext_addr_equal(mac.dst_ext, s_central_ext_addr) &&
                       mac.src_mode == IOT154_ADDR_MODE_EXT) {
                if (is_duplicate(packet.device_id, packet.seq)) {
                    ESP_LOGI(TAG, "DATA duplicate dev=0x%08" PRIx32 " seq=%u", packet.device_id, packet.seq);
                } else {
                    ESP_LOGI(TAG,
                             "DATA new dev=0x%08" PRIx32 " seq=%u event=%u value=%u",
                             packet.device_id, packet.seq, packet.event_type, packet.value);
                }
                send_ack(packet.device_id, packet.seq, mac.src_ext);
            } else {
                ESP_LOGW(TAG, "ignored frame: msg=%u not for this central", packet.msg_type);
            }
        }

        if ((bits & TX_DONE_BIT) != 0) {
            ESP_LOGI(TAG, "%s TX done seq=%u", s_tx_type, s_ack_tx_seq);
            ESP_ERROR_CHECK(iot154_radio_start_rx());
        }

        if ((bits & TX_FAILED_BIT) != 0) {
            ESP_LOGW(TAG, "%s TX failed seq=%u error=%d", s_tx_type, s_ack_tx_seq, s_ack_tx_error);
            ESP_ERROR_CHECK(iot154_radio_start_rx());
        }
    }
}
