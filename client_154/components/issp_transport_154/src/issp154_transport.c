#include "issp154_transport.h"

#include "esp_attr.h"
#include "esp_ieee802154.h"
#include "iot154_radio.h"

static issp154_transport_rx_done_cb_t s_rx_done_cb;
static issp154_transport_tx_done_cb_t s_tx_done_cb;
static issp154_transport_tx_failed_cb_t s_tx_failed_cb;
static void *s_context;

static void IRAM_ATTR transport_rx_done(uint8_t *frame, esp_ieee802154_frame_info_t *frame_info)
{
    if (s_rx_done_cb != NULL) {
        s_rx_done_cb(frame, frame_info, s_context);
    }
}

static void IRAM_ATTR transport_tx_done(const uint8_t *frame,
                                        const uint8_t *ack,
                                        esp_ieee802154_frame_info_t *ack_info)
{
    if (s_tx_done_cb != NULL) {
        s_tx_done_cb(frame, ack, ack_info, s_context);
    }
}

static void IRAM_ATTR transport_tx_failed(const uint8_t *frame, esp_ieee802154_tx_error_t error)
{
    if (s_tx_failed_cb != NULL) {
        s_tx_failed_cb(frame, error, s_context);
    }
}

esp_err_t issp154_transport_init(const issp154_transport_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_rx_done_cb = config->rx_done_cb;
    s_tx_done_cb = config->tx_done_cb;
    s_tx_failed_cb = config->tx_failed_cb;
    s_context = config->context;

    return iot154_radio_init(config->channel,
                             config->pan_id,
                             config->short_address,
                             config->coordinator,
                             transport_rx_done,
                             transport_tx_done,
                             transport_tx_failed);
}

esp_err_t issp154_transport_set_extended_address(const uint8_t *extended_address)
{
    return esp_ieee802154_set_extended_address(extended_address);
}

esp_err_t issp154_transport_start(void)
{
    return iot154_radio_start_rx();
}

esp_err_t issp154_transport_sleep(void)
{
    return esp_ieee802154_sleep();
}

esp_err_t issp154_transport_send(const uint8_t *frame, bool cca)
{
    return esp_ieee802154_transmit(frame, cca);
}

void issp154_transport_release_receive_buffer(const uint8_t *frame)
{
    esp_ieee802154_receive_handle_done((uint8_t *)frame);
}
