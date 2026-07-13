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

struct IsspReport
{
    std::uint8_t endpointId;
    std::uint8_t eventType;
    std::uint8_t value;
};

struct IsspDeviceConfig
{
    std::uint32_t deviceId;
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
