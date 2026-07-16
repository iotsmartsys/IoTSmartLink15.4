#pragma once

#include <cstddef>
#include <cstdint>

#include "issp_types.hpp"

namespace issp
{

class IIsspTransport
{
public:
    /// replyContext is opaque and valid only for the duration of the handler call.
    using ReceiveHandler = void (*)(const std::uint8_t *data,
                                    std::size_t length,
                                    const void *replyContext,
                                    void *context);

    virtual IsspResult begin() = 0;
    virtual IsspResult send(const std::uint8_t *data, std::size_t length) = 0;
    virtual IsspTransportState state() const = 0;
    virtual void setReceiveHandler(ReceiveHandler handler, void *context) = 0;

    virtual ~IIsspTransport() = default;
};

} // namespace issp
