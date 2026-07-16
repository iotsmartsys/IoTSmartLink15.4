#include "issp154_mac_frame.h"

#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#else
#define IRAM_ATTR
#endif

enum {
    ISSP154_PHY_MAX_LENGTH = 127,
    ISSP154_FCS_LENGTH = 2,
    ISSP154_FCF_LENGTH = 2,
    ISSP154_SEQUENCE_LENGTH = 1,
    ISSP154_PAN_ID_LENGTH = 2,
    ISSP154_SHORT_ADDRESS_LENGTH = 2,
    ISSP154_EXTENDED_ADDRESS_LENGTH = 8,
    ISSP154_FRAME_TYPE_DATA = 1,
    ISSP154_ADDRESS_MODE_NONE = 0,
    ISSP154_ADDRESS_MODE_SHORT = 2,
    ISSP154_ADDRESS_MODE_EXTENDED = 3,
};

static bool IRAM_ATTR consume_field(size_t field_length, size_t payload_end, size_t *position)
{
    if (*position > payload_end || field_length > payload_end - *position) {
        return false;
    }

    *position += field_length;
    return true;
}

static bool IRAM_ATTR address_length(uint8_t mode, size_t *length)
{
    switch (mode) {
    case ISSP154_ADDRESS_MODE_NONE:
        *length = 0;
        return true;
    case ISSP154_ADDRESS_MODE_SHORT:
        *length = ISSP154_SHORT_ADDRESS_LENGTH;
        return true;
    case ISSP154_ADDRESS_MODE_EXTENDED:
        *length = ISSP154_EXTENDED_ADDRESS_LENGTH;
        return true;
    default:
        return false;
    }
}

static uint16_t IRAM_ATTR read_uint16_little_endian(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

bool IRAM_ATTR issp154_mac_extract_payload_and_source(const uint8_t *frame,
                                                      size_t frame_buffer_length,
                                                      const uint8_t **payload,
                                                      size_t *payload_length,
                                                      issp154_mac_source_t *source)
{
    if (payload == NULL || payload_length == NULL || source == NULL) {
        return false;
    }

    *payload = NULL;
    *payload_length = 0;
    memset(source, 0, sizeof(*source));

    if (frame == NULL || frame_buffer_length < 1) {
        return false;
    }

    const uint8_t physical_length = frame[0];
    if (physical_length == 0 || physical_length > ISSP154_PHY_MAX_LENGTH) {
        return false;
    }

    const size_t declared_buffer_length = (size_t)physical_length + 1;
    if (frame_buffer_length < declared_buffer_length ||
        physical_length < ISSP154_FCF_LENGTH + ISSP154_SEQUENCE_LENGTH + ISSP154_FCS_LENGTH) {
        return false;
    }

    const size_t payload_end = declared_buffer_length - ISSP154_FCS_LENGTH;
    const uint16_t fcf = (uint16_t)frame[1] | ((uint16_t)frame[2] << 8);
    const uint8_t frame_type = (uint8_t)(fcf & 0x07U);
    const bool security_enabled = (fcf & (1U << 3)) != 0;
    const bool pan_compression = (fcf & (1U << 6)) != 0;
    const bool sequence_suppression = (fcf & (1U << 8)) != 0;
    const uint8_t destination_mode = (uint8_t)((fcf >> 10) & 0x03U);
    const uint8_t frame_version = (uint8_t)((fcf >> 12) & 0x03U);
    const uint8_t source_mode = (uint8_t)((fcf >> 14) & 0x03U);

    if (frame_type != ISSP154_FRAME_TYPE_DATA || security_enabled ||
        sequence_suppression || frame_version > 1) {
        return false;
    }

    size_t destination_address_length = 0;
    size_t source_address_length = 0;
    if (!address_length(destination_mode, &destination_address_length) ||
        !address_length(source_mode, &source_address_length)) {
        return false;
    }

    if (source_mode == ISSP154_ADDRESS_MODE_NONE) {
        return false;
    }

    if (pan_compression &&
        (destination_mode == ISSP154_ADDRESS_MODE_NONE || source_mode == ISSP154_ADDRESS_MODE_NONE)) {
        return false;
    }

    size_t position = 1 + ISSP154_FCF_LENGTH + ISSP154_SEQUENCE_LENGTH;
    uint16_t destination_pan_id = 0;
    if (destination_mode != ISSP154_ADDRESS_MODE_NONE) {
        if (!consume_field(ISSP154_PAN_ID_LENGTH, payload_end, &position)) {
            return false;
        }
        destination_pan_id = read_uint16_little_endian(&frame[position - ISSP154_PAN_ID_LENGTH]);
        if (!consume_field(destination_address_length, payload_end, &position)) {
            return false;
        }
    }

    if (pan_compression) {
        source->source_pan_id = destination_pan_id;
    } else {
        if (!consume_field(ISSP154_PAN_ID_LENGTH, payload_end, &position)) {
            return false;
        }
        source->source_pan_id = read_uint16_little_endian(&frame[position - ISSP154_PAN_ID_LENGTH]);
    }

    if (!consume_field(source_address_length, payload_end, &position)) {
        return false;
    }
    source->source_address_mode = source_mode;
    memcpy(source->source_address, &frame[position - source_address_length], source_address_length);

    if (position > payload_end) {
        return false;
    }

    *payload = &frame[position];
    *payload_length = payload_end - position;
    return true;
}

bool IRAM_ATTR issp154_mac_build_reply(const issp154_mac_source_t *destination,
                                       uint16_t local_pan_id,
                                       uint16_t local_short_address,
                                       const uint8_t *local_extended_address,
                                       uint8_t sequence,
                                       const uint8_t *payload,
                                       size_t payload_length,
                                       uint8_t *frame,
                                       size_t frame_capacity,
                                       size_t *frame_length)
{
    if (destination == NULL || payload == NULL || payload_length == 0 ||
        frame == NULL || frame_length == NULL) {
        return false;
    }

    size_t address_field_length = 0;
    if (!address_length(destination->source_address_mode, &address_field_length) ||
        destination->source_address_mode == ISSP154_ADDRESS_MODE_NONE) {
        return false;
    }

    if (destination->source_address_mode == ISSP154_ADDRESS_MODE_SHORT) {
        const uint16_t destination_short = read_uint16_little_endian(destination->source_address);
        if (destination_short >= 0xfffeU || local_short_address >= 0xfffeU) {
            return false;
        }
    } else if (local_extended_address == NULL) {
        return false;
    }

    const bool pan_compression = destination->source_pan_id == local_pan_id;
    const size_t mac_header_length = ISSP154_FCF_LENGTH + ISSP154_SEQUENCE_LENGTH +
                                     ISSP154_PAN_ID_LENGTH + address_field_length +
                                     (pan_compression ? 0 : ISSP154_PAN_ID_LENGTH) +
                                     address_field_length;
    const size_t physical_length = mac_header_length + payload_length + ISSP154_FCS_LENGTH;
    const size_t total_length = physical_length + 1;
    if (physical_length > ISSP154_PHY_MAX_LENGTH || total_length > frame_capacity) {
        return false;
    }

    const uint16_t fcf = (uint16_t)(ISSP154_FRAME_TYPE_DATA |
                                    (pan_compression ? (1U << 6) : 0U) |
                                    ((uint16_t)destination->source_address_mode << 10) |
                                    (1U << 12) |
                                    ((uint16_t)destination->source_address_mode << 14));

    size_t position = 0;
    frame[position++] = (uint8_t)physical_length;
    frame[position++] = (uint8_t)fcf;
    frame[position++] = (uint8_t)(fcf >> 8);
    frame[position++] = sequence;
    frame[position++] = (uint8_t)destination->source_pan_id;
    frame[position++] = (uint8_t)(destination->source_pan_id >> 8);
    memcpy(&frame[position], destination->source_address, address_field_length);
    position += address_field_length;

    if (!pan_compression) {
        frame[position++] = (uint8_t)local_pan_id;
        frame[position++] = (uint8_t)(local_pan_id >> 8);
    }

    if (destination->source_address_mode == ISSP154_ADDRESS_MODE_SHORT) {
        frame[position++] = (uint8_t)local_short_address;
        frame[position++] = (uint8_t)(local_short_address >> 8);
    } else {
        memcpy(&frame[position], local_extended_address, ISSP154_EXTENDED_ADDRESS_LENGTH);
        position += ISSP154_EXTENDED_ADDRESS_LENGTH;
    }

    memcpy(&frame[position], payload, payload_length);
    position += payload_length;
    frame[position++] = 0;
    frame[position++] = 0;
    *frame_length = position;
    return true;
}
