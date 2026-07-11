#include "issp154_transport.h"

#include "esp_ieee802154.h"
#include "iot154_radio.h"

esp_err_t issp154_transport_init(const issp154_transport_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return iot154_radio_init(config->channel,
                             config->pan_id,
                             config->short_address,
                             config->coordinator,
                             config->rx_done_cb,
                             config->tx_done_cb,
                             config->tx_failed_cb);
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
