#pragma once

#include <cstddef>
#include <cstdint>

#include "issp_types.hpp"

namespace issp
{

constexpr std::size_t IsspPayloadSize = 12;

IsspResult decodeCommand(
    const std::uint8_t *data,
    std::size_t length,
    std::uint32_t expectedDeviceId,
    IsspDecodedCommand &decodedCommand);

IsspResult encodeCommandAck(
    std::uint32_t deviceId,
    std::uint16_t sequence,
    std::uint8_t endpointId,
    IsspCommandResult commandResult,
    std::uint8_t *output,
    std::size_t outputCapacity,
    std::size_t &outputLength);

} // namespace issp
