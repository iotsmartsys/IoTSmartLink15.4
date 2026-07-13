#pragma once

#include <cstddef>
#include <cstdint>

#include "issp_types.hpp"

namespace issp
{

class IIsspTransport
{
public:
    using ReceiveHandler = void (*)(const std::uint8_t *data, std::size_t length, void *context);

    virtual IsspResult begin() = 0;
    virtual IsspResult send(const std::uint8_t *data, std::size_t length) = 0;
    virtual IsspTransportState state() const = 0;
    virtual void setReceiveHandler(ReceiveHandler handler, void *context) = 0;

    virtual ~IIsspTransport() = default;
};

} // namespace issp
