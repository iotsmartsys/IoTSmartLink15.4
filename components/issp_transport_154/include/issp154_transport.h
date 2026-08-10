#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_ieee802154_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*issp154_transport_rx_done_cb_t)(uint8_t *frame,
                                               esp_ieee802154_frame_info_t *frame_info,
                                               void *context);
typedef void (*issp154_transport_tx_done_cb_t)(const uint8_t *frame,
                                               const uint8_t *ack,
                                               esp_ieee802154_frame_info_t *ack_info,
                                               void *context);
typedef void (*issp154_transport_tx_failed_cb_t)(const uint8_t *frame,
                                                 esp_ieee802154_tx_error_t error,
                                                 void *context);

typedef struct {
    uint8_t channel;
    uint16_t pan_id;
    uint16_t short_address;
    bool coordinator;
    bool promiscuous;
    issp154_transport_rx_done_cb_t rx_done_cb;
    issp154_transport_tx_done_cb_t tx_done_cb;
    issp154_transport_tx_failed_cb_t tx_failed_cb;
    void *context;
    /// Keep false for the legacy ISR callback; true delivers copied RX frames from a task.
    bool defer_rx_to_task;
} issp154_transport_config_t;

/**
 * @brief Initialize and configure the IEEE 802.15.4 radio.
 *
 * The underlying driver and this API support one active radio instance. The
 * opaque context is forwarded to callbacks for that active instance; multiple
 * simultaneous contexts are not supported.
 */
esp_err_t issp154_transport_init(const issp154_transport_config_t *config);

/// @brief Release the radio and transport resources initialized by init.
esp_err_t issp154_transport_deinit(void);

/// @brief Configure the local IEEE 802.15.4 extended address.
esp_err_t issp154_transport_set_extended_address(const uint8_t *extended_address);

/// @brief Put the radio in continuous receive mode.
esp_err_t issp154_transport_start(void);

/// @brief Put the radio into sleep before a physical transmission.
esp_err_t issp154_transport_sleep(void);

/// @brief Transmit an already serialized IEEE 802.15.4 frame.
esp_err_t issp154_transport_send(const uint8_t *frame, bool cca);

/**
 * @brief Transmit one frame and wait only for its physical TX completion.
 *
 * The caller must keep frame valid until this function returns. ESP_OK means
 * TX_DONE only; it does not indicate a MAC or ISSP acknowledgement. After a
 * timeout, new synchronous transmissions return ESP_ERR_INVALID_STATE until
 * the pending physical completion callback is received.
 */
esp_err_t issp154_transport_transmit_and_wait(const uint8_t *frame,
                                              bool cca,
                                              uint32_t timeout_ms);

/// @brief Return whether a synchronous physical transmission is still active.
bool issp154_transport_is_synchronous_transmit_busy(void);

/// @brief Return the number of received frames dropped by the deferred RX queue.
uint32_t issp154_transport_rx_drop_count(void);

/// @brief Return a received frame buffer to the IEEE 802.15.4 driver.
void issp154_transport_release_receive_buffer(const uint8_t *frame);

#ifdef __cplusplus
}
#endif
