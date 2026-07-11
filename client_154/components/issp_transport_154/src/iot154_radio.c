#include "iot154_radio.h"

#include "esp_check.h"
#include "esp_ieee802154.h"

static iot154_rx_done_cb_t s_rx_cb;
static iot154_tx_done_cb_t s_tx_done_cb;
static iot154_tx_failed_cb_t s_tx_failed_cb;

static void IRAM_ATTR radio_rx_done(uint8_t *frame, esp_ieee802154_frame_info_t *frame_info)
{
    if (s_rx_cb != NULL) {
        s_rx_cb(frame, frame_info);
    }
}

static void IRAM_ATTR radio_tx_done(const uint8_t *frame, const uint8_t *ack, esp_ieee802154_frame_info_t *ack_info)
{
    if (s_tx_done_cb != NULL) {
        s_tx_done_cb(frame, ack, ack_info);
    }
}

static void IRAM_ATTR radio_tx_failed(const uint8_t *frame, esp_ieee802154_tx_error_t error)
{
    if (s_tx_failed_cb != NULL) {
        s_tx_failed_cb(frame, error);
    }
}

esp_err_t iot154_radio_init(uint8_t channel,
                            uint16_t pan_id,
                            uint16_t short_addr,
                            bool coordinator,
                            iot154_rx_done_cb_t rx_cb,
                            iot154_tx_done_cb_t tx_done_cb,
                            iot154_tx_failed_cb_t tx_failed_cb)
{
    s_rx_cb = rx_cb;
    s_tx_done_cb = tx_done_cb;
    s_tx_failed_cb = tx_failed_cb;

    esp_ieee802154_event_cb_list_t callbacks = {
        .rx_done_cb = radio_rx_done,
        .tx_done_cb = radio_tx_done,
        .tx_failed_cb = radio_tx_failed,
    };

    ESP_RETURN_ON_ERROR(esp_ieee802154_event_callback_list_register(callbacks), "iot154", "register callbacks");
    ESP_RETURN_ON_ERROR(esp_ieee802154_enable(), "iot154", "enable radio");
    ESP_RETURN_ON_ERROR(esp_ieee802154_set_channel(channel), "iot154", "set channel");
    ESP_RETURN_ON_ERROR(esp_ieee802154_set_panid(pan_id), "iot154", "set pan");
    ESP_RETURN_ON_ERROR(esp_ieee802154_set_short_address(short_addr), "iot154", "set short address");
    ESP_RETURN_ON_ERROR(esp_ieee802154_set_coordinator(coordinator), "iot154", "set coordinator");
    ESP_RETURN_ON_ERROR(esp_ieee802154_set_promiscuous(false), "iot154", "set promiscuous");
    ESP_RETURN_ON_ERROR(esp_ieee802154_set_rx_when_idle(true), "iot154", "set rx idle");
    return ESP_OK;
}

esp_err_t iot154_radio_start_rx(void)
{
    ESP_RETURN_ON_ERROR(esp_ieee802154_receive(), "iot154", "receive");
    return esp_ieee802154_set_rx_when_idle(true);
}
