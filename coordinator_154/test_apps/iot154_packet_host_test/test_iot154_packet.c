/* Golden wire vectors for the coordinator codec (REPORT-ID-AC-004, AC-015) and
 * the canonical event_id formatting (REPORT-ID-AC-011).
 *
 * The client keeps a separate codec and a literally identical vector table in
 * components/issp_core/test_apps/issp_protocol_host_test. The vectors are the
 * shared artifact; the code is deliberately not shared across the two target
 * directories. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iot154_packet.h"

static unsigned s_tests;

#define CHECK(condition)                                                                \
    do                                                                                  \
    {                                                                                   \
        if (!(condition))                                                               \
        {                                                                               \
            fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__,           \
                    #condition);                                                        \
            exit(1);                                                                    \
        }                                                                               \
    } while (0)

#define VECTOR_SIZE 24

static const uint8_t k_data_typical[VECTOR_SIZE] = {
    0x03, 0x01, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD, 0xAB, 0x89,
    0x67, 0x45, 0x23, 0x01, 0x01, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x22};
static const uint8_t k_data_min_identity[VECTOR_SIZE] = {
    0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05};
static const uint8_t k_data_max_boundary[VECTOR_SIZE] = {
    0x03, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xF3};
static const uint8_t k_report_ack[VECTOR_SIZE] = {
    0x03, 0x02, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD, 0xAB, 0x89,
    0x67, 0x45, 0x23, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20};
static const uint8_t k_command_ack[VECTOR_SIZE] = {
    0x03, 0x02, 0x02, 0x00, 0x40, 0x15, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66};
static const uint8_t k_discovery_request[VECTOR_SIZE] = {
    0x03, 0x03, 0x02, 0x00, 0x40, 0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5E};
static const uint8_t k_discovery_response[VECTOR_SIZE] = {
    0x03, 0x04, 0x02, 0x00, 0x40, 0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5F};
static const uint8_t k_command[VECTOR_SIZE] = {
    0x03, 0x05, 0x02, 0x00, 0x40, 0x15, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x6B};

#define DEVICE_ID 0x15400002u
#define REPORT_ID 0x0123456789ABCDEFull

static void build(iot154_packet_t *packet,
                  uint8_t msg_type,
                  uint32_t device_id,
                  uint16_t seq,
                  uint64_t report_id,
                  uint8_t endpoint_id,
                  uint8_t event_type,
                  uint8_t value)
{
    memset(packet, 0, sizeof(*packet));
    packet->version = IOT154_VERSION;
    packet->msg_type = msg_type;
    packet->device_id = device_id;
    packet->seq = seq;
    packet->report_id = report_id;
    packet->endpoint_id = endpoint_id;
    packet->event_type = event_type;
    packet->value = value;
    iot154_packet_finalize(packet);
}

static bool packet_matches(const iot154_packet_t *packet, const uint8_t *expected)
{
    uint8_t encoded[VECTOR_SIZE];
    iot154_packet_encode(packet, encoded);
    return memcmp(encoded, expected, VECTOR_SIZE) == 0;
}

static void layout_matches_the_contract(void)
{
    CHECK(IOT154_PAYLOAD_LEN == VECTOR_SIZE);
    CHECK(IOT154_VERSION == 3);
    /* The three frame shapes of this target with a v3 payload stay below the
       IEEE 802.15.4 limit of 127 bytes. */
    CHECK(IOT154_MAC_HEADER_LEN + VECTOR_SIZE + IOT154_FCS_LEN <= IOT154_MAX_FRAME_LEN);
    CHECK(IOT154_MAC_HEADER_SHORT_EXT_LEN + VECTOR_SIZE + IOT154_FCS_LEN <= IOT154_MAX_FRAME_LEN);
    CHECK(IOT154_MAC_HEADER_EXT_LEN + VECTOR_SIZE + IOT154_FCS_LEN <= IOT154_MAX_FRAME_LEN);
    ++s_tests;
}

static void encoders_match_the_golden_vectors(void)
{
    iot154_packet_t packet;

    build(&packet, IOT154_MSG_DATA, DEVICE_ID, 0x0102, REPORT_ID, 1, 2, 1);
    CHECK(packet_matches(&packet, k_data_typical));

    build(&packet, IOT154_MSG_DATA, 0, 0, 1, 0, 0, 0);
    CHECK(packet_matches(&packet, k_data_min_identity));

    build(&packet, IOT154_MSG_DATA, 0xFFFFFFFFu, 0xFFFFu, 0xFFFFFFFFFFFFFFFFull, 0xFF, 0xFF, 0xFF);
    CHECK(packet_matches(&packet, k_data_max_boundary));

    build(&packet, IOT154_MSG_ACK, DEVICE_ID, 0x0102, REPORT_ID, 1, 0, IOT154_ACK_STATUS_OK);
    CHECK(packet_matches(&packet, k_report_ack));

    build(&packet, IOT154_MSG_ACK, DEVICE_ID, 0x0007, 0, 3, 0, IOT154_ACK_STATUS_OK);
    CHECK(packet_matches(&packet, k_command_ack));

    build(&packet, IOT154_MSG_DISCOVERY_REQ, DEVICE_ID, 0x0001, 0, 0, 0, 0);
    CHECK(packet_matches(&packet, k_discovery_request));

    build(&packet, IOT154_MSG_DISCOVERY_RESP, DEVICE_ID, 0x0001, 0, 0, 0, IOT154_ACK_STATUS_OK);
    CHECK(packet_matches(&packet, k_discovery_response));

    build(&packet, IOT154_MSG_CMD, DEVICE_ID, 0x0007, 0, 3, 1, 1);
    CHECK(packet_matches(&packet, k_command));
    ++s_tests;
}

static void decoders_read_the_golden_vectors(void)
{
    iot154_packet_t packet;

    iot154_packet_decode(k_data_typical, &packet);
    CHECK(iot154_packet_is_valid(&packet));
    CHECK(packet.device_id == DEVICE_ID);
    CHECK(packet.seq == 0x0102);
    CHECK(packet.report_id == REPORT_ID);
    CHECK(packet.endpoint_id == 1 && packet.event_type == 2 && packet.value == 1);

    iot154_packet_decode(k_data_max_boundary, &packet);
    CHECK(iot154_packet_is_valid(&packet));
    CHECK(packet.report_id == 0xFFFFFFFFFFFFFFFFull);

    /* The single wire ACK type: non-zero identity is a report ACK, zero is a
       command ACK. Both are valid frames. */
    iot154_packet_decode(k_report_ack, &packet);
    CHECK(iot154_packet_is_valid(&packet) && packet.report_id == REPORT_ID);
    iot154_packet_decode(k_command_ack, &packet);
    CHECK(iot154_packet_is_valid(&packet) && packet.report_id == 0);
    ++s_tests;
}

static void invalid_versions_and_combinations_are_rejected(void)
{
    iot154_packet_t packet;

    /* A v1 frame padded to the v3 length is still refused by version. */
    iot154_packet_decode(k_data_typical, &packet);
    packet.version = 1;
    iot154_packet_finalize(&packet);
    CHECK(!iot154_packet_is_valid(&packet));

    /* DATA with a zero identity is not a valid report. */
    build(&packet, IOT154_MSG_DATA, DEVICE_ID, 0x0102, 0, 1, 2, 1);
    CHECK(!iot154_packet_is_valid(&packet));

    /* A non-report type carrying a non-zero identity is an invalid frame. */
    build(&packet, IOT154_MSG_CMD, DEVICE_ID, 0x0007, 1, 3, 1, 1);
    CHECK(!iot154_packet_is_valid(&packet));
    build(&packet, IOT154_MSG_DISCOVERY_REQ, DEVICE_ID, 0x0001, 1, 0, 0, 0);
    CHECK(!iot154_packet_is_valid(&packet));
    build(&packet, IOT154_MSG_DISCOVERY_RESP, DEVICE_ID, 0x0001, 1, 0, 0, 0);
    CHECK(!iot154_packet_is_valid(&packet));

    /* A corrupted checksum is refused. */
    iot154_packet_decode(k_data_typical, &packet);
    packet.checksum = (uint8_t)(packet.checksum + 1u);
    CHECK(!iot154_packet_is_valid(&packet));
    ++s_tests;
}

static void frame_lengths_other_than_the_payload_are_rejected(void)
{
    uint8_t frame[IOT154_MAX_FRAME_LEN + 1];
    iot154_frame_info_t info;
    iot154_packet_t packet;
    iot154_packet_t decoded;

    build(&packet, IOT154_MSG_DATA, DEVICE_ID, 0x0102, REPORT_ID, 1, 2, 1);
    memset(frame, 0, sizeof(frame));
    const size_t frame_length = iot154_build_frame(frame, 0x1234, 0x0000, 7, &packet);
    CHECK(frame_length == IOT154_MAC_HEADER_LEN + VECTOR_SIZE + IOT154_FCS_LEN + 1u);

    /* The exact v3 length is accepted. */
    CHECK(iot154_parse_frame_info(frame, &info, &decoded));
    CHECK(decoded.report_id == REPORT_ID);

    /* One byte short of the fixed payload is refused. */
    const uint8_t declared_length = frame[0];
    frame[0] = (uint8_t)(declared_length - 1u);
    CHECK(!iot154_parse_frame_info(frame, &info, &decoded));

    /* One byte beyond it is refused as well: a longer frame whose first twenty-four
       payload bytes form a valid v3 packet must not be accepted. */
    frame[0] = (uint8_t)(declared_length + 1u);
    CHECK(!iot154_parse_frame_info(frame, &info, &decoded));

    frame[0] = declared_length;
    CHECK(iot154_parse_frame_info(frame, &info, &decoded));
    ++s_tests;
}

static void event_id_is_canonical(void)
{
    char out[64];

    CHECK(iot154_format_event_id("issp154-00124B0000000001", REPORT_ID, out, sizeof(out)));
    CHECK(strcmp(out, "issp154-00124B0000000001:0123456789ABCDEF") == 0);

    /* Sixteen uppercase digits, zero padded, for the boundary identities. */
    CHECK(iot154_format_event_id("issp154-00124B0000000001", 1u, out, sizeof(out)));
    CHECK(strcmp(out, "issp154-00124B0000000001:0000000000000001") == 0);
    CHECK(iot154_format_event_id("issp154-00124B0000000001", 0xFFFFFFFFFFFFFFFFull, out,
                                 sizeof(out)));
    CHECK(strcmp(out, "issp154-00124B0000000001:FFFFFFFFFFFFFFFF") == 0);

    /* A buffer that cannot hold the whole identity yields nothing, so a
       truncated identity is never emitted. */
    char tight[41];
    CHECK(!iot154_format_event_id("issp154-00124B0000000001", REPORT_ID, tight, sizeof(tight)));
    char exact[42];
    CHECK(iot154_format_event_id("issp154-00124B0000000001", REPORT_ID, exact, sizeof(exact)));
    ++s_tests;
}


/* TV-AC-001/002/003: independent typed vectors and both codec/frame paths. */
static const struct { unsigned type; uint32_t bits; uint8_t wire[24]; } typed_vectors[] = {
    {0, 0x00000000U, {0x03, 0x01, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0x01, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E}},
    {0, 0x00000001U, {0x03, 0x01, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0x01, 0xFF, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1F}},
    {0, 0xFFFFFFFFU, {0x03, 0x01, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0x01, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x1A}},
    {0, 0x80000000U, {0x03, 0x01, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0x01, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x80, 0x9E}},
    {0, 0x7FFFFFFFU, {0x03, 0x01, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0x01, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x7F, 0x9A}},
    {0, 0x01000001U, {0x03, 0x01, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0x01, 0xFF, 0x00, 0x01, 0x00, 0x00, 0x01, 0x20}},
    {1, 0x00000000U, {0x03, 0x01, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0x01, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x1F}},
    {1, 0x3F000000U, {0x03, 0x01, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0x01, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x3F, 0x5E}},
    {1, 0x4283BD71U, {0x03, 0x01, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0x01, 0xFF, 0x01, 0x71, 0xBD, 0x83, 0x42, 0x12}},
    {1, 0x7F7FFFFFU, {0x03, 0x01, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0x01, 0xFF, 0x01, 0xFF, 0xFF, 0x7F, 0x7F, 0x1B}},
    {1, 0xFF7FFFFFU, {0x03, 0x01, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0x01, 0xFF, 0x01, 0xFF, 0xFF, 0x7F, 0xFF, 0x9B}},
    {1, 0x00000001U, {0x03, 0x01, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0x01, 0xFF, 0x01, 0x01, 0x00, 0x00, 0x00, 0x20}},
};
static void typed_vectors_and_all_mac_shapes(void)
{
    const uint8_t address[8] = {1,2,3,4,5,6,7,8};
    for (size_t i=0; i<sizeof(typed_vectors)/sizeof(typed_vectors[0]); ++i) {
        iot154_packet_t packet;
        build(&packet,IOT154_MSG_DATA,DEVICE_ID,0x0102,REPORT_ID,1,255,0);
        packet.value_type = (uint8_t)typed_vectors[i].type;
        packet.value = typed_vectors[i].bits;
        iot154_packet_finalize(&packet);
        CHECK(packet_matches(&packet,typed_vectors[i].wire));
        iot154_packet_decode(typed_vectors[i].wire,&packet);
        CHECK(iot154_packet_is_valid(&packet));
        CHECK(packet.value_type == typed_vectors[i].type && packet.value == typed_vectors[i].bits);
    }
    const uint8_t *messages[] = {k_data_typical,k_report_ack,k_command_ack,k_discovery_request,k_discovery_response,k_command};
    for (size_t i=0;i<sizeof(messages)/sizeof(messages[0]);++i) {
        iot154_packet_t packet;
        iot154_packet_decode(messages[i],&packet);
        for (unsigned shape=0;shape<3;++shape) {
            uint8_t frame[128]={0};
            size_t length;
            if (shape == 0) length=iot154_build_frame(frame,1,2,1,&packet);
            else if (shape == 1) length=iot154_build_ext_frame(frame,address,address,1,&packet);
            else length=iot154_build_broadcast_from_ext_frame(frame,address,1,&packet);
            CHECK(length == (shape==0?9u:(shape==1?21u:15u))+24+2+1);
            iot154_packet_t decoded;
            iot154_frame_info_t info;
            CHECK(iot154_parse_frame_info(frame,&info,&decoded));
            CHECK(packet_matches(&decoded,messages[i]));
            --frame[0]; CHECK(!iot154_parse_frame_info(frame,&info,&decoded));
            frame[0]+=2; CHECK(!iot154_parse_frame_info(frame,&info,&decoded));
        }
    }
    ++s_tests;
}
static void invalid_typed_content(void)
{
    const uint32_t invalid[] = {0x7f800000,0xff800000,0x7fc00000,0x80000000};
    iot154_packet_t packet;
    for (size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        build(&packet,IOT154_MSG_DATA,DEVICE_ID,1,REPORT_ID,1,255,0);
        packet.value_type=1; packet.value=invalid[i]; iot154_packet_finalize(&packet);
        CHECK(!iot154_packet_is_valid(&packet));
    }
    for (uint8_t kind=1;kind<=5;++kind) {
        build(&packet,kind,DEVICE_ID,1,kind==1?REPORT_ID:0,1,255,0);
        packet.value_type=kind==1?2:1; iot154_packet_finalize(&packet);
        CHECK(!iot154_packet_is_valid(&packet));
    }
    build(&packet,99,DEVICE_ID,1,0,1,255,0);
    CHECK(!iot154_packet_is_valid(&packet));
    build(&packet,IOT154_MSG_DATA,DEVICE_ID,1,REPORT_ID,1,255,0);
    packet.version=2; iot154_packet_finalize(&packet);
    CHECK(!iot154_packet_is_valid(&packet));
    ++s_tests;
}

int main(void)
{
    typed_vectors_and_all_mac_shapes();
    invalid_typed_content();
    layout_matches_the_contract();
    encoders_match_the_golden_vectors();
    decoders_read_the_golden_vectors();
    invalid_versions_and_combinations_are_rejected();
    frame_lengths_other_than_the_payload_are_rejected();
    event_id_is_canonical();
    printf("%u Tests 0 Failures 0 Ignored\n", s_tests);
    return 0;
}
