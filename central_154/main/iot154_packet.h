#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IOT154_CHANNEL 15
#define IOT154_PAN_ID 0x1540
#define IOT154_CENTRAL_ADDR 0x0001
#define IOT154_SENSOR_ADDR 0x1001
#define IOT154_BROADCAST_ADDR 0xffff
#define IOT154_VERSION 1

#define IOT154_MSG_DATA 1
#define IOT154_MSG_ACK 2
#define IOT154_MSG_DISCOVERY_REQ 3
#define IOT154_MSG_DISCOVERY_RESP 4
#define IOT154_EVENT_DOOR 1
#define IOT154_EVENT_POWER 2
#define IOT154_EVENT_BATTERY_LEVEL_PERCENT 3
#define IOT154_ACK_STATUS_OK 0
#define IOT154_VALUE_OFF 0
#define IOT154_VALUE_ON 1
#define IOT154_VALUE_TOGGLE 2

#define IOT154_MAC_HEADER_LEN 9
#define IOT154_MAC_HEADER_SHORT_EXT_LEN 15
#define IOT154_MAC_HEADER_EXT_LEN 21
#define IOT154_FCS_LEN 2
#define IOT154_MAX_FRAME_LEN 127
#define IOT154_EXT_ADDR_LEN 8
#define IOT154_SENSOR_DEVICE_ID 0x15400001

#define IOT154_ADDR_MODE_NONE 0
#define IOT154_ADDR_MODE_SHORT 2
#define IOT154_ADDR_MODE_EXT 3

typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t msg_type;
    uint32_t device_id;
    uint16_t seq;
    uint8_t event_type;
    uint8_t value;
    uint8_t checksum;
} iot154_packet_t;

typedef struct {
    uint8_t src_mode;
    uint8_t dst_mode;
    uint16_t src_short;
    uint16_t dst_short;
    uint8_t src_ext[IOT154_EXT_ADDR_LEN];
    uint8_t dst_ext[IOT154_EXT_ADDR_LEN];
    bool dst_broadcast;
} iot154_frame_info_t;

/// @brief Return an 8-bit additive checksum over packet bytes except checksum.
static inline uint8_t iot154_checksum(const iot154_packet_t *packet)
{
    const uint8_t *bytes = (const uint8_t *)packet;
    uint8_t sum = 0;
    for (size_t i = 0; i < sizeof(*packet) - 1; ++i) {
        sum = (uint8_t)(sum + bytes[i]);
    }
    return sum;
}

/// @brief Fill checksum after all other packet fields are set.
static inline void iot154_packet_finalize(iot154_packet_t *packet)
{
    packet->checksum = iot154_checksum(packet);
}

/// @brief Validate protocol version and checksum.
static inline bool iot154_packet_is_valid(const iot154_packet_t *packet)
{
    return packet->version == IOT154_VERSION && packet->checksum == iot154_checksum(packet);
}

/// @brief Return true when two 802.15.4 extended addresses match.
static inline bool iot154_ext_addr_equal(const uint8_t *a, const uint8_t *b)
{
    return memcmp(a, b, IOT154_EXT_ADDR_LEN) == 0;
}

/// @brief Build a basic 802.15.4 data frame with short addresses and PAN compression.
static inline size_t iot154_build_frame(uint8_t *frame,
                                        uint16_t src_addr,
                                        uint16_t dst_addr,
                                        uint8_t mac_seq,
                                        const iot154_packet_t *packet)
{
    const uint16_t fcf = 0x9841;
    frame[0] = IOT154_MAC_HEADER_LEN + sizeof(*packet) + IOT154_FCS_LEN;
    frame[1] = (uint8_t)(fcf & 0xff);
    frame[2] = (uint8_t)(fcf >> 8);
    frame[3] = mac_seq;
    frame[4] = (uint8_t)(IOT154_PAN_ID & 0xff);
    frame[5] = (uint8_t)(IOT154_PAN_ID >> 8);
    frame[6] = (uint8_t)(dst_addr & 0xff);
    frame[7] = (uint8_t)(dst_addr >> 8);
    frame[8] = (uint8_t)(src_addr & 0xff);
    frame[9] = (uint8_t)(src_addr >> 8);
    memcpy(&frame[1 + IOT154_MAC_HEADER_LEN], packet, sizeof(*packet));
    return frame[0] + 1;
}

/// @brief Build a data frame with extended source and destination addresses.
static inline size_t iot154_build_ext_frame(uint8_t *frame,
                                            const uint8_t *src_ext,
                                            const uint8_t *dst_ext,
                                            uint8_t mac_seq,
                                            const iot154_packet_t *packet)
{
    const uint16_t fcf = 0xdc41;
    frame[0] = IOT154_MAC_HEADER_EXT_LEN + sizeof(*packet) + IOT154_FCS_LEN;
    frame[1] = (uint8_t)(fcf & 0xff);
    frame[2] = (uint8_t)(fcf >> 8);
    frame[3] = mac_seq;
    frame[4] = (uint8_t)(IOT154_PAN_ID & 0xff);
    frame[5] = (uint8_t)(IOT154_PAN_ID >> 8);
    memcpy(&frame[6], dst_ext, IOT154_EXT_ADDR_LEN);
    memcpy(&frame[14], src_ext, IOT154_EXT_ADDR_LEN);
    memcpy(&frame[1 + IOT154_MAC_HEADER_EXT_LEN], packet, sizeof(*packet));
    return frame[0] + 1;
}

/// @brief Build a broadcast discovery frame with short broadcast destination and extended source.
static inline size_t iot154_build_broadcast_from_ext_frame(uint8_t *frame,
                                                           const uint8_t *src_ext,
                                                           uint8_t mac_seq,
                                                           const iot154_packet_t *packet)
{
    const uint16_t fcf = 0xd841;
    frame[0] = IOT154_MAC_HEADER_SHORT_EXT_LEN + sizeof(*packet) + IOT154_FCS_LEN;
    frame[1] = (uint8_t)(fcf & 0xff);
    frame[2] = (uint8_t)(fcf >> 8);
    frame[3] = mac_seq;
    frame[4] = (uint8_t)(IOT154_PAN_ID & 0xff);
    frame[5] = (uint8_t)(IOT154_PAN_ID >> 8);
    frame[6] = (uint8_t)(IOT154_BROADCAST_ADDR & 0xff);
    frame[7] = (uint8_t)(IOT154_BROADCAST_ADDR >> 8);
    memcpy(&frame[8], src_ext, IOT154_EXT_ADDR_LEN);
    memcpy(&frame[1 + IOT154_MAC_HEADER_SHORT_EXT_LEN], packet, sizeof(*packet));
    return frame[0] + 1;
}

/// @brief Extract addressing metadata and protocol payload from an 802.15.4 frame.
static inline bool iot154_parse_frame_info(const uint8_t *frame, iot154_frame_info_t *info, iot154_packet_t *packet)
{
    if (frame[0] < IOT154_MAC_HEADER_LEN + sizeof(*packet) + IOT154_FCS_LEN) {
        return false;
    }

    const uint16_t fcf = (uint16_t)frame[1] | ((uint16_t)frame[2] << 8);
    const uint8_t dst_mode = (uint8_t)((fcf >> 10) & 0x03);
    const uint8_t src_mode = (uint8_t)((fcf >> 14) & 0x03);
    const bool pan_compression = (fcf & (1U << 6)) != 0;
    size_t pos = 4;

    memset(info, 0, sizeof(*info));
    info->src_mode = src_mode;
    info->dst_mode = dst_mode;

    if (dst_mode != IOT154_ADDR_MODE_NONE) {
        if (pos + 2 > frame[0] + 1) {
            return false;
        }
        const uint16_t dst_pan = (uint16_t)frame[pos] | ((uint16_t)frame[pos + 1] << 8);
        pos += 2;
        if (dst_pan != IOT154_PAN_ID) {
            return false;
        }

        if (dst_mode == IOT154_ADDR_MODE_SHORT) {
            if (pos + 2 > frame[0] + 1) {
                return false;
            }
            info->dst_short = (uint16_t)frame[pos] | ((uint16_t)frame[pos + 1] << 8);
            info->dst_broadcast = info->dst_short == IOT154_BROADCAST_ADDR;
            pos += 2;
        } else if (dst_mode == IOT154_ADDR_MODE_EXT) {
            if (pos + IOT154_EXT_ADDR_LEN > frame[0] + 1) {
                return false;
            }
            memcpy(info->dst_ext, &frame[pos], IOT154_EXT_ADDR_LEN);
            pos += IOT154_EXT_ADDR_LEN;
        } else {
            return false;
        }
    }

    if (src_mode != IOT154_ADDR_MODE_NONE) {
        if (!pan_compression) {
            if (pos + 2 > frame[0] + 1) {
                return false;
            }
            const uint16_t src_pan = (uint16_t)frame[pos] | ((uint16_t)frame[pos + 1] << 8);
            pos += 2;
            if (src_pan != IOT154_PAN_ID) {
                return false;
            }
        }

        if (src_mode == IOT154_ADDR_MODE_SHORT) {
            if (pos + 2 > frame[0] + 1) {
                return false;
            }
            info->src_short = (uint16_t)frame[pos] | ((uint16_t)frame[pos + 1] << 8);
            pos += 2;
        } else if (src_mode == IOT154_ADDR_MODE_EXT) {
            if (pos + IOT154_EXT_ADDR_LEN > frame[0] + 1) {
                return false;
            }
            memcpy(info->src_ext, &frame[pos], IOT154_EXT_ADDR_LEN);
            pos += IOT154_EXT_ADDR_LEN;
        } else {
            return false;
        }
    }

    const size_t mac_header_len = pos - 1;
    if (frame[0] < mac_header_len + sizeof(*packet) + IOT154_FCS_LEN) {
        return false;
    }

    memcpy(packet, &frame[pos], sizeof(*packet));
    return iot154_packet_is_valid(packet);
}

/// @brief Extract protocol payload from a received short-address 802.15.4 frame.
static inline bool iot154_parse_frame(const uint8_t *frame,
                                      uint16_t *src_addr,
                                      uint16_t *dst_addr,
                                      iot154_packet_t *packet)
{
    iot154_frame_info_t info = {0};
    if (!iot154_parse_frame_info(frame, &info, packet)) {
        return false;
    }

    if (dst_addr != NULL) {
        *dst_addr = info.dst_short;
    }
    if (src_addr != NULL) {
        *src_addr = info.src_short;
    }

    return info.src_mode == IOT154_ADDR_MODE_SHORT && info.dst_mode == IOT154_ADDR_MODE_SHORT;
}

#ifdef __cplusplus
}
#endif
