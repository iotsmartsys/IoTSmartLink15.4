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
    std::uint64_t reportId;
    IsspReport report;
};

/// Source of report identities, injected by the composition root. It must
/// return a 64-bit value and is called outside every critical section, so it
/// may block briefly. Returning zero or a value already held by an occupied
/// slot makes the device retry within a bounded local search.
using ReportIdGenerator = std::uint64_t (*)(void *context);

struct IsspDeviceConfig
{
    std::uint32_t deviceId;
    /// Required for operational v2 construction: a configuration without a
    /// generator rejects every report admission.
    ReportIdGenerator reportIdGenerator;
    void *reportIdGeneratorContext;
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

/// The wire ACK type is single. A non-zero reportId identifies the ACK of a
/// report and echoes its identity; zero identifies the ACK of a command.
struct IsspDecodedAck
{
    std::uint32_t deviceId;
    std::uint16_t sequence;
    std::uint64_t reportId;
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
