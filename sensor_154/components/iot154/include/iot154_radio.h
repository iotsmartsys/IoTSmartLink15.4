#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_ieee802154_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*iot154_rx_done_cb_t)(uint8_t *frame, esp_ieee802154_frame_info_t *frame_info);
typedef void (*iot154_tx_done_cb_t)(const uint8_t *frame, const uint8_t *ack, esp_ieee802154_frame_info_t *ack_info);
typedef void (*iot154_tx_failed_cb_t)(const uint8_t *frame, esp_ieee802154_tx_error_t error);

/// @brief Initialize IEEE 802.15.4 radio with fixed PAN, short address and channel.
esp_err_t iot154_radio_init(uint16_t short_addr,
                            bool coordinator,
                            iot154_rx_done_cb_t rx_cb,
                            iot154_tx_done_cb_t tx_done_cb,
                            iot154_tx_failed_cb_t tx_failed_cb);

/// @brief Put radio in continuous receive mode.
esp_err_t iot154_radio_start_rx(void);

#ifdef __cplusplus
}
#endif
