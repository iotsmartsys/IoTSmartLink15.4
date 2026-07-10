#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t attempts;
    int64_t tx_start_us;
    int64_t ack_received_us;
} iot154_sensor_tx_result_t;

esp_err_t iot154_sensor_client_init(const uint8_t *sensor_ext_addr);
void iot154_sensor_client_set_central_ext_addr(const uint8_t *central_ext_addr);
bool iot154_sensor_client_discover_central(uint16_t seq, uint8_t *central_ext_addr);
bool iot154_sensor_client_transmit_data_with_ack(uint16_t seq,
                                                 uint8_t event_type,
                                                 uint8_t value,
                                                 iot154_sensor_tx_result_t *result);

#ifdef __cplusplus
}
#endif
