#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t source_address_mode;
    uint16_t source_pan_id;
    uint8_t source_address[8];
} issp154_mac_source_t;

bool issp154_mac_extract_payload_and_source(const uint8_t *frame,
                                            size_t frame_buffer_length,
                                            const uint8_t **payload,
                                            size_t *payload_length,
                                            issp154_mac_source_t *source);

bool issp154_mac_build_reply(const issp154_mac_source_t *destination,
                             uint16_t local_pan_id,
                             uint16_t local_short_address,
                             const uint8_t *local_extended_address,
                             uint8_t sequence,
                             const uint8_t *payload,
                             size_t payload_length,
                             uint8_t *frame,
                             size_t frame_capacity,
                             size_t *frame_length);

#ifdef __cplusplus
}
#endif
