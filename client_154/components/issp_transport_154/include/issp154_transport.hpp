#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "iissp_transport.hpp"
#include "issp154_transport.h"

namespace issp
{

inline constexpr std::size_t kIssp154ExtendedAddressSize = 8;
inline constexpr std::size_t kIssp154FrameCapacity = 128;

struct Issp154TransportConfig
{
    std::uint8_t channel;
    std::uint16_t panId;
    std::uint16_t shortAddress;
    bool coordinator;
    const std::uint8_t *extendedAddress;
    bool cca;
};

class Issp154Transport final : public IIsspTransport
{
public:
    explicit Issp154Transport(const Issp154TransportConfig &config);

    IsspResult setDestination(const std::uint8_t *extendedAddress,
                              std::size_t length);
    void clearDestination();
    bool hasDestination() const;

    IsspResult begin() override;
    IsspResult send(const std::uint8_t *data, std::size_t length) override;
    IsspResult sendReply(const std::uint8_t *data,
                         std::size_t length,
                         const void *replyContext) override;
    IsspTransportState state() const override;
    /// Data and the opaque reply context are valid only during the handler call.
    void setReceiveHandler(ReceiveHandler handler, void *context) override;

private:
    static void handleRxDone(std::uint8_t *frame,
                             esp_ieee802154_frame_info_t *frameInfo,
                             void *context);
    static void handleTxDone(const std::uint8_t *frame,
                             const std::uint8_t *ack,
                             esp_ieee802154_frame_info_t *ackInfo,
                             void *context);
    static void handleTxFailed(const std::uint8_t *frame,
                               esp_ieee802154_tx_error_t error,
                               void *context);

    void notifyReceive(std::uint8_t *frame);

    Issp154TransportConfig config_;
    ReceiveHandler receiveHandler_;
    void *receiveContext_;
    IsspTransportState state_;
    std::uint8_t macSequence_;
    std::array<std::uint8_t, kIssp154FrameCapacity> txFrame_;
    std::array<std::uint8_t, kIssp154FrameCapacity> replyFrame_;
    std::array<std::uint8_t, kIssp154ExtendedAddressSize> destination_;
    bool hasDestination_;
};

} // namespace issp
