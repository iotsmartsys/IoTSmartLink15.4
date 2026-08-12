// Golden wire vectors for the client codec (REPORT-ID-AC-004, AC-015).
//
// The coordinator keeps a separate codec and a literally identical vector
// table in coordinator_154/test_apps/iot154_packet_host_test. The vectors are
// the shared artifact; the code is deliberately not shared across the two
// target directories.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "issp_protocol.hpp"

namespace
{

unsigned g_tests;

#define CHECK(condition)                                                                \
    do                                                                                  \
    {                                                                                   \
        if (!(condition))                                                               \
        {                                                                               \
            std::fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__,     \
                         #condition);                                                   \
            std::exit(1);                                                               \
        }                                                                               \
    } while (0)

constexpr std::size_t kVectorSize = 20;
static_assert(issp::IsspPayloadSize == kVectorSize,
              "ISSP v2 payload must be 20 bytes");

// Largest existing frame shape of this target: extended source and extended
// destination. 21 bytes of MAC header plus the payload plus the 2-byte FCS.
constexpr std::size_t kExtendedMacHeaderBytes = 21;
constexpr std::size_t kFcsBytes = 2;
constexpr std::size_t kMacFrameLimit = 127;
static_assert(kExtendedMacHeaderBytes + kVectorSize + kFcsBytes <= kMacFrameLimit,
              "largest ISSP v2 frame must fit the IEEE 802.15.4 MAC limit");

const std::uint8_t kDataTypical[kVectorSize] = {
    0x02, 0x01, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD,
    0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0x01, 0x02, 0x01, 0x21};
const std::uint8_t kDataMinIdentity[kVectorSize] = {
    0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04};
const std::uint8_t kDataMaxBoundary[kVectorSize] = {
    0x02, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF2};
const std::uint8_t kReportAck[kVectorSize] = {
    0x02, 0x02, 0x02, 0x00, 0x40, 0x15, 0x02, 0x01, 0xEF, 0xCD,
    0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0x01, 0x00, 0x00, 0x1F};
const std::uint8_t kCommandAck[kVectorSize] = {
    0x02, 0x02, 0x02, 0x00, 0x40, 0x15, 0x07, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x65};
const std::uint8_t kDiscoveryRequest[kVectorSize] = {
    0x02, 0x03, 0x02, 0x00, 0x40, 0x15, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5D};
const std::uint8_t kDiscoveryResponse[kVectorSize] = {
    0x02, 0x04, 0x02, 0x00, 0x40, 0x15, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5E};
const std::uint8_t kCommand[kVectorSize] = {
    0x02, 0x05, 0x02, 0x00, 0x40, 0x15, 0x07, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x01, 0x6A};

constexpr std::uint32_t kDeviceId = 0x15400002;
constexpr std::uint64_t kReportId = 0x0123456789ABCDEFULL;

void encoders_match_the_golden_vectors()
{
    std::uint8_t buffer[kVectorSize] = {};
    std::size_t length = 0;

    CHECK(issp::encodeReport(kDeviceId, 0x0102, kReportId,
                             {.endpointId = 1, .eventType = 2, .value = 1},
                             buffer, sizeof(buffer), length) == issp::IsspResult::Ok);
    CHECK(length == kVectorSize);
    CHECK(std::memcmp(buffer, kDataTypical, kVectorSize) == 0);

    CHECK(issp::encodeReport(0, 0, 1, {.endpointId = 0, .eventType = 0, .value = 0},
                             buffer, sizeof(buffer), length) == issp::IsspResult::Ok);
    CHECK(std::memcmp(buffer, kDataMinIdentity, kVectorSize) == 0);

    CHECK(issp::encodeReport(0xFFFFFFFF, 0xFFFF, 0xFFFFFFFFFFFFFFFFULL,
                             {.endpointId = 0xFF, .eventType = 0xFF, .value = 0xFF},
                             buffer, sizeof(buffer), length) == issp::IsspResult::Ok);
    CHECK(std::memcmp(buffer, kDataMaxBoundary, kVectorSize) == 0);

    CHECK(issp::encodeCommandAck(kDeviceId, 0x0007, 3, issp::IsspCommandResult::Accepted,
                                 buffer, sizeof(buffer), length) == issp::IsspResult::Ok);
    CHECK(std::memcmp(buffer, kCommandAck, kVectorSize) == 0);

    CHECK(issp::encodeDiscoveryRequest(kDeviceId, 0x0001, buffer, sizeof(buffer), length) ==
          issp::IsspResult::Ok);
    CHECK(std::memcmp(buffer, kDiscoveryRequest, kVectorSize) == 0);
    ++g_tests;
}

void decoders_read_the_golden_vectors()
{
    issp::IsspDecodedReport report{};
    CHECK(issp::decodeReport(kDataTypical, kVectorSize, report) == issp::IsspResult::Ok);
    CHECK(report.deviceId == kDeviceId);
    CHECK(report.sequence == 0x0102);
    CHECK(report.reportId == kReportId);
    CHECK(report.report.endpointId == 1 && report.report.eventType == 2 &&
          report.report.value == 1);

    CHECK(issp::decodeReport(kDataMaxBoundary, kVectorSize, report) == issp::IsspResult::Ok);
    CHECK(report.reportId == 0xFFFFFFFFFFFFFFFFULL);

    // The single wire ACK type: a non-zero identity is a report ACK, zero is a
    // command ACK. Both decode, and the identity is what tells them apart.
    issp::IsspDecodedAck ack{};
    CHECK(issp::decodeAck(kReportAck, kVectorSize, ack) == issp::IsspResult::Ok);
    CHECK(ack.reportId == kReportId && ack.endpointId == 1 &&
          ack.status == issp::IsspAckStatus::Ok);
    CHECK(issp::decodeAck(kCommandAck, kVectorSize, ack) == issp::IsspResult::Ok);
    CHECK(ack.reportId == 0 && ack.endpointId == 3);

    issp::IsspDecodedCommand command{};
    CHECK(issp::decodeCommand(kCommand, kVectorSize, kDeviceId, command) ==
          issp::IsspResult::Ok);
    CHECK(command.sequence == 0x0007 && command.command.endpointId == 3);

    issp::IsspDecodedDiscoveryResponse response{};
    CHECK(issp::decodeDiscoveryResponse(kDiscoveryResponse, kVectorSize, response) ==
          issp::IsspResult::Ok);
    CHECK(response.deviceId == kDeviceId && response.sequence == 0x0001);
    ++g_tests;
}

void v1_frames_and_wrong_lengths_are_rejected()
{
    // A v1 frame: 12 bytes, version byte 1. Rejected by length, and rejected by
    // version even when padded to the v2 length.
    const std::uint8_t v1Report[12] = {0x01, 0x01, 0x02, 0x00, 0x40, 0x15,
                                       0x02, 0x01, 0x01, 0x02, 0x01, 0x64};
    issp::IsspDecodedReport report{};
    CHECK(issp::decodeReport(v1Report, sizeof(v1Report), report) ==
          issp::IsspResult::InvalidArgument);

    std::uint8_t padded[kVectorSize] = {};
    std::memcpy(padded, v1Report, sizeof(v1Report));
    padded[19] = 0;
    std::uint8_t sum = 0;
    for (std::size_t i = 0; i < 19; ++i)
    {
        sum = static_cast<std::uint8_t>(sum + padded[i]);
    }
    padded[19] = sum;
    CHECK(issp::decodeReport(padded, kVectorSize, report) == issp::IsspResult::Failed);

    // Truncation by one byte in either direction is refused.
    CHECK(issp::decodeReport(kDataTypical, kVectorSize - 1, report) ==
          issp::IsspResult::InvalidArgument);
    CHECK(issp::decodeReport(kDataTypical, kVectorSize + 1, report) ==
          issp::IsspResult::InvalidArgument);
    ++g_tests;
}

void invalid_type_and_identity_combinations_are_rejected()
{
    // DATA with a zero identity is not a valid report.
    std::uint8_t zeroIdentityData[kVectorSize] = {};
    std::memcpy(zeroIdentityData, kDataTypical, kVectorSize);
    std::memset(&zeroIdentityData[8], 0, 8);
    std::uint8_t sum = 0;
    for (std::size_t i = 0; i < 19; ++i)
    {
        sum = static_cast<std::uint8_t>(sum + zeroIdentityData[i]);
    }
    zeroIdentityData[19] = sum;
    issp::IsspDecodedReport report{};
    CHECK(issp::decodeReport(zeroIdentityData, kVectorSize, report) ==
          issp::IsspResult::Failed);

    // Encoding a report with a zero identity is refused at the source.
    std::uint8_t buffer[kVectorSize] = {};
    std::size_t length = 0;
    CHECK(issp::encodeReport(kDeviceId, 1, 0, {.endpointId = 1, .eventType = 2, .value = 1},
                             buffer, sizeof(buffer), length) ==
          issp::IsspResult::InvalidArgument);

    // A non-report type carrying a non-zero identity is an invalid frame.
    std::uint8_t identifiedCommand[kVectorSize] = {};
    std::memcpy(identifiedCommand, kCommand, kVectorSize);
    identifiedCommand[8] = 0x01;
    sum = 0;
    for (std::size_t i = 0; i < 19; ++i)
    {
        sum = static_cast<std::uint8_t>(sum + identifiedCommand[i]);
    }
    identifiedCommand[19] = sum;
    issp::IsspDecodedCommand command{};
    CHECK(issp::decodeCommand(identifiedCommand, kVectorSize, kDeviceId, command) ==
          issp::IsspResult::Failed);

    std::uint8_t identifiedResponse[kVectorSize] = {};
    std::memcpy(identifiedResponse, kDiscoveryResponse, kVectorSize);
    identifiedResponse[8] = 0x01;
    sum = 0;
    for (std::size_t i = 0; i < 19; ++i)
    {
        sum = static_cast<std::uint8_t>(sum + identifiedResponse[i]);
    }
    identifiedResponse[19] = sum;
    issp::IsspDecodedDiscoveryResponse response{};
    CHECK(issp::decodeDiscoveryResponse(identifiedResponse, kVectorSize, response) ==
          issp::IsspResult::Failed);

    // A corrupted checksum is refused.
    std::uint8_t corrupted[kVectorSize] = {};
    std::memcpy(corrupted, kDataTypical, kVectorSize);
    corrupted[19] = static_cast<std::uint8_t>(corrupted[19] + 1U);
    CHECK(issp::decodeReport(corrupted, kVectorSize, report) == issp::IsspResult::Failed);
    ++g_tests;
}

} // namespace

int main()
{
    encoders_match_the_golden_vectors();
    decoders_read_the_golden_vectors();
    v1_frames_and_wrong_lengths_are_rejected();
    invalid_type_and_identity_combinations_are_rejected();
    std::printf("%u Tests 0 Failures 0 Ignored\n", g_tests);
    return 0;
}
