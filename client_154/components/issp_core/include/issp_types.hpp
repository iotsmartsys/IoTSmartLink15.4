#pragma once

#include <cstdint>

namespace issp
{

struct IsspCommand
{
    std::uint8_t endpointId;
    std::uint8_t eventType;
    std::uint8_t value;
};

struct IsspDecodedCommand
{
    IsspCommand command;
    std::uint16_t sequence;
};

struct IsspReport
{
    std::uint8_t endpointId;
    std::uint8_t eventType;
    std::uint8_t value;
};

struct IsspPendingReportToken
{
    std::uint8_t slotIndex;
    std::uint32_t generation;
};

struct IsspDecodedReport
{
    std::uint32_t deviceId;
    std::uint16_t sequence;
    IsspReport report;
};

struct IsspDeviceConfig
{
    std::uint32_t deviceId;
};

enum class IsspCommandResult : std::uint8_t
{
    Accepted,
    Unsupported,
    Invalid,
    Failed,
};

enum class IsspAckStatus : std::uint8_t
{
    Ok,
    Unsupported,
    Invalid,
};

struct IsspDecodedAck
{
    std::uint32_t deviceId;
    std::uint16_t sequence;
    std::uint8_t endpointId;
    IsspAckStatus status;
};

struct IsspDecodedDiscoveryResponse
{
    std::uint32_t deviceId;
    std::uint16_t sequence;
    std::uint8_t endpointId;
    IsspAckStatus status;
};

enum class IsspTransportState : std::uint8_t
{
    Stopped,
    Starting,
    Ready,
    Error,
};

enum class IsspResult : std::uint8_t
{
    Ok,
    InvalidArgument,
    NotReady,
    Busy,
    Failed,
};

} // namespace issp
