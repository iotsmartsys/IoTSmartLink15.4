#include "issp_protocol.hpp"

namespace issp
{
namespace
{

// Mirrors the current packed ISSP wire format during the C-to-C++ transition.
constexpr std::uint8_t kProtocolVersion = 1;
constexpr std::uint8_t kAckMessageType = 2;
constexpr std::uint8_t kCommandMessageType = 5;
constexpr std::size_t kVersionOffset = 0;
constexpr std::size_t kMessageTypeOffset = 1;
constexpr std::size_t kDeviceIdOffset = 2;
constexpr std::size_t kSequenceOffset = 6;
constexpr std::size_t kEndpointIdOffset = 8;
constexpr std::size_t kEventTypeOffset = 9;
constexpr std::size_t kValueOffset = 10;
constexpr std::size_t kChecksumOffset = 11;

constexpr std::uint8_t kAckStatusOk = 0;
constexpr std::uint8_t kAckStatusUnsupported = 1;
constexpr std::uint8_t kAckStatusInvalid = 2;

std::uint32_t readUint32LittleEndian(const std::uint8_t *data)
{
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8U) |
           (static_cast<std::uint32_t>(data[2]) << 16U) |
           (static_cast<std::uint32_t>(data[3]) << 24U);
}

std::uint16_t readUint16LittleEndian(const std::uint8_t *data)
{
    return static_cast<std::uint16_t>(data[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8U);
}

void writeUint32LittleEndian(std::uint8_t *output, std::uint32_t value)
{
    output[0] = static_cast<std::uint8_t>(value);
    output[1] = static_cast<std::uint8_t>(value >> 8U);
    output[2] = static_cast<std::uint8_t>(value >> 16U);
    output[3] = static_cast<std::uint8_t>(value >> 24U);
}

void writeUint16LittleEndian(std::uint8_t *output, std::uint16_t value)
{
    output[0] = static_cast<std::uint8_t>(value);
    output[1] = static_cast<std::uint8_t>(value >> 8U);
}

std::uint8_t calculateChecksum(const std::uint8_t *data, std::size_t length)
{
    std::uint8_t sum = 0;
    for (std::size_t index = 0; index < length; ++index)
    {
        sum = static_cast<std::uint8_t>(sum + data[index]);
    }
    return sum;
}

IsspAckStatus commandResultToAckStatus(IsspCommandResult commandResult)
{
    switch (commandResult)
    {
    case IsspCommandResult::Accepted:
        return IsspAckStatus::Ok;
    case IsspCommandResult::Unsupported:
        return IsspAckStatus::Unsupported;
    case IsspCommandResult::Invalid:
        return IsspAckStatus::Invalid;
    case IsspCommandResult::Failed:
        // The current wire protocol has no failure status; INVALID is the
        // conservative response for a command that could not be completed.
        return IsspAckStatus::Invalid;
    }
    return IsspAckStatus::Invalid;
}

std::uint8_t ackStatusToWireValue(IsspAckStatus status)
{
    switch (status)
    {
    case IsspAckStatus::Ok:
        return kAckStatusOk;
    case IsspAckStatus::Unsupported:
        return kAckStatusUnsupported;
    case IsspAckStatus::Invalid:
        return kAckStatusInvalid;
    }
    return kAckStatusInvalid;
}

} // namespace

IsspResult decodeCommand(
    const std::uint8_t *data,
    std::size_t length,
    std::uint32_t expectedDeviceId,
    IsspDecodedCommand &decodedCommand)
{
    if (data == nullptr || length != IsspPayloadSize)
    {
        return IsspResult::InvalidArgument;
    }

    if (data[kVersionOffset] != kProtocolVersion ||
        data[kChecksumOffset] != calculateChecksum(data, kChecksumOffset) ||
        data[kMessageTypeOffset] != kCommandMessageType ||
        readUint32LittleEndian(&data[kDeviceIdOffset]) != expectedDeviceId)
    {
        return IsspResult::Failed;
    }

    const IsspDecodedCommand decoded{
        .command = {
            .endpointId = data[kEndpointIdOffset],
            .eventType = data[kEventTypeOffset],
            .value = data[kValueOffset],
        },
        .sequence = readUint16LittleEndian(&data[kSequenceOffset]),
    };
    decodedCommand = decoded;
    return IsspResult::Ok;
}

IsspResult encodeCommandAck(
    std::uint32_t deviceId,
    std::uint16_t sequence,
    std::uint8_t endpointId,
    IsspCommandResult commandResult,
    std::uint8_t *output,
    std::size_t outputCapacity,
    std::size_t &outputLength)
{
    if (output == nullptr || outputCapacity < IsspPayloadSize)
    {
        return IsspResult::InvalidArgument;
    }

    output[kVersionOffset] = kProtocolVersion;
    output[kMessageTypeOffset] = kAckMessageType;
    writeUint32LittleEndian(&output[kDeviceIdOffset], deviceId);
    writeUint16LittleEndian(&output[kSequenceOffset], sequence);
    output[kEndpointIdOffset] = endpointId;
    output[kEventTypeOffset] = 0;
    output[kValueOffset] = ackStatusToWireValue(commandResultToAckStatus(commandResult));
    output[kChecksumOffset] = calculateChecksum(output, kChecksumOffset);
    outputLength = IsspPayloadSize;
    return IsspResult::Ok;
}

} // namespace issp
