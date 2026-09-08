#pragma once

#include <cstddef>
#include <cstdint>

#include "issp_types.hpp"

namespace issp
{

/// ISSP v3 fixed payload with explicit Int32/Float32 content.
/// Older versions are rejected without fallback or translation.
constexpr std::size_t IsspPayloadSize = 24;

IsspResult encodeDiscoveryRequest(
    std::uint32_t deviceId,
    std::uint16_t sequence,
    std::uint8_t *output,
    std::size_t outputCapacity,
    std::size_t &outputLength);

IsspResult decodeDiscoveryResponse(
    const std::uint8_t *data,
    std::size_t length,
    IsspDecodedDiscoveryResponse &decodedResponse);

IsspResult decodeCommand(
    const std::uint8_t *data,
    std::size_t length,
    std::uint32_t expectedDeviceId,
    IsspDecodedCommand &decodedCommand);

IsspResult decodeAck(
    const std::uint8_t *data,
    std::size_t length,
    IsspDecodedAck &decodedAck);

IsspResult decodeAck(
    const std::uint8_t *data,
    std::size_t length,
    std::uint32_t expectedDeviceId,
    IsspDecodedAck &decodedAck);

IsspResult decodeReport(
    const std::uint8_t *data,
    std::size_t length,
    IsspDecodedReport &decodedReport);

IsspResult decodeReport(
    const std::uint8_t *data,
    std::size_t length,
    std::uint32_t expectedDeviceId,
    IsspDecodedReport &decodedReport);

IsspResult encodeCommandAck(
    std::uint32_t deviceId,
    std::uint16_t sequence,
    std::uint8_t endpointId,
    IsspCommandResult commandResult,
    std::uint8_t *output,
    std::size_t outputCapacity,
    std::size_t &outputLength);

/// A report carries its logical identity: reportId must be non-zero.
IsspResult encodeReport(
    std::uint32_t deviceId,
    std::uint16_t sequence,
    std::uint64_t reportId,
    const IsspReport &report,
    std::uint8_t *output,
    std::size_t outputCapacity,
    std::size_t &outputLength);

} // namespace issp
