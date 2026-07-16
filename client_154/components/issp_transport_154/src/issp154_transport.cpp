#include "issp154_transport.hpp"

#include <algorithm>

#include "esp_attr.h"
#include "issp154_mac_frame.h"
#include "issp154_transport.h"

namespace issp
{

namespace
{

constexpr std::uint32_t kPhysicalTxTimeoutMs = 100;

IsspResult mapEspError(esp_err_t error)
{
    switch (error) {
    case ESP_OK:
        return IsspResult::Ok;
    case ESP_ERR_INVALID_ARG:
        return IsspResult::InvalidArgument;
    case ESP_ERR_INVALID_STATE:
        return IsspResult::NotReady;
    default:
        return IsspResult::Failed;
    }
}

IsspResult mapTransmitError(esp_err_t error)
{
    switch (error) {
    case ESP_OK:
        return IsspResult::Ok;
    case ESP_ERR_INVALID_ARG:
        return IsspResult::InvalidArgument;
    case ESP_ERR_INVALID_STATE:
        return IsspResult::Busy;
    default:
        return IsspResult::Failed;
    }
}

} // namespace

Issp154Transport::Issp154Transport(const Issp154TransportConfig &config)
    : config_(config),
      receiveHandler_(nullptr),
      receiveContext_(nullptr),
      state_(IsspTransportState::Stopped),
      macSequence_(0),
      txFrame_{},
      replyFrame_{},
      destination_{},
      hasDestination_(false)
{
}

IsspResult Issp154Transport::setDestination(const std::uint8_t *extendedAddress,
                                            std::size_t length)
{
    if (extendedAddress == nullptr || length != destination_.size()) {
        return IsspResult::InvalidArgument;
    }

    std::copy_n(extendedAddress, destination_.size(), destination_.begin());
    hasDestination_ = true;
    return IsspResult::Ok;
}

void Issp154Transport::clearDestination()
{
    destination_.fill(0);
    hasDestination_ = false;
}

bool Issp154Transport::hasDestination() const
{
    return hasDestination_;
}

IsspResult Issp154Transport::begin()
{
    if (config_.extendedAddress == nullptr) {
        state_ = IsspTransportState::Error;
        return IsspResult::InvalidArgument;
    }

    state_ = IsspTransportState::Starting;

    const issp154_transport_config_t transportConfig = {
        .channel = config_.channel,
        .pan_id = config_.panId,
        .short_address = config_.shortAddress,
        .coordinator = config_.coordinator,
        .rx_done_cb = &Issp154Transport::handleRxDone,
        .tx_done_cb = &Issp154Transport::handleTxDone,
        .tx_failed_cb = &Issp154Transport::handleTxFailed,
        .context = this,
        .defer_rx_to_task = true,
    };

    esp_err_t error = issp154_transport_init(&transportConfig);
    if (error != ESP_OK) {
        state_ = IsspTransportState::Error;
        return mapEspError(error);
    }

    error = issp154_transport_set_extended_address(config_.extendedAddress);
    if (error != ESP_OK) {
        state_ = IsspTransportState::Error;
        return mapEspError(error);
    }

    error = issp154_transport_start();
    if (error != ESP_OK) {
        state_ = IsspTransportState::Error;
        return mapEspError(error);
    }

    state_ = IsspTransportState::Ready;
    return IsspResult::Ok;
}

IsspResult Issp154Transport::send(const std::uint8_t *data, std::size_t length)
{
    if (data == nullptr || length == 0) {
        return IsspResult::InvalidArgument;
    }

    if (state_ != IsspTransportState::Ready) {
        return IsspResult::NotReady;
    }

    if (!hasDestination_) {
        return IsspResult::NotReady;
    }

    // The current transport contract is serial. Checking before writing keeps
    // a frame retained by the C layer after timeout from being overwritten.
    if (issp154_transport_is_synchronous_transmit_busy()) {
        return IsspResult::Busy;
    }

    std::size_t frameLength = 0;
    const std::uint8_t sequence = macSequence_;
    if (!issp154_mac_build_extended_unicast(config_.panId,
                                             destination_.data(),
                                             config_.extendedAddress,
                                             sequence,
                                             data,
                                             length,
                                             txFrame_.data(),
                                             txFrame_.size(),
                                             &frameLength)) {
        return IsspResult::InvalidArgument;
    }

    if (frameLength != static_cast<std::size_t>(txFrame_[0]) + 1U) {
        return IsspResult::Failed;
    }

    ++macSequence_;
    return mapTransmitError(issp154_transport_transmit_and_wait(
        txFrame_.data(), config_.cca, kPhysicalTxTimeoutMs));
}

IsspResult Issp154Transport::sendReply(const std::uint8_t *data,
                                       std::size_t length,
                                       const void *replyContext)
{
    if (data == nullptr || length == 0 || replyContext == nullptr) {
        return IsspResult::InvalidArgument;
    }

    if (state_ != IsspTransportState::Ready) {
        return IsspResult::NotReady;
    }

    // The transport contract is serial. This also protects a reply frame kept
    // alive for a physical completion callback after a timeout.
    if (issp154_transport_is_synchronous_transmit_busy()) {
        return IsspResult::Busy;
    }

    const auto *destination = static_cast<const issp154_mac_source_t *>(replyContext);
    std::size_t frameLength = 0;
    const std::uint8_t sequence = macSequence_;
    if (!issp154_mac_build_reply(
            destination,
            config_.panId,
            config_.shortAddress,
            config_.extendedAddress,
            sequence,
            data,
            length,
            replyFrame_.data(),
            replyFrame_.size(),
            &frameLength)) {
        return IsspResult::Failed;
    }

    if (frameLength != static_cast<std::size_t>(replyFrame_[0]) + 1U) {
        return IsspResult::Failed;
    }

    ++macSequence_;
    return mapTransmitError(issp154_transport_transmit_and_wait(
        replyFrame_.data(), config_.cca, kPhysicalTxTimeoutMs));
}

IsspTransportState Issp154Transport::state() const
{
    return state_;
}

void Issp154Transport::setReceiveHandler(ReceiveHandler handler, void *context)
{
    receiveHandler_ = handler;
    receiveContext_ = context;
}

void IRAM_ATTR Issp154Transport::handleRxDone(std::uint8_t *frame,
                                              esp_ieee802154_frame_info_t *frameInfo,
                                              void *context)
{
    (void)frameInfo;

    if (context == nullptr) {
        return;
    }

    auto *instance = static_cast<Issp154Transport *>(context);
    instance->notifyReceive(frame);
}

void IRAM_ATTR Issp154Transport::handleTxDone(const std::uint8_t *frame,
                                              const std::uint8_t *ack,
                                              esp_ieee802154_frame_info_t *ackInfo,
                                              void *context)
{
    (void)frame;
    (void)ackInfo;

    if (context == nullptr) {
        if (ack != nullptr) {
            issp154_transport_release_receive_buffer(ack);
        }
        return;
    }

    (void)static_cast<Issp154Transport *>(context);
    if (ack != nullptr) {
        issp154_transport_release_receive_buffer(ack);
    }
}

void IRAM_ATTR Issp154Transport::handleTxFailed(const std::uint8_t *frame,
                                                esp_ieee802154_tx_error_t error,
                                                void *context)
{
    (void)frame;
    (void)error;

    if (context == nullptr) {
        return;
    }

    (void)static_cast<Issp154Transport *>(context);
}

void IRAM_ATTR Issp154Transport::notifyReceive(std::uint8_t *frame)
{
    if (frame == nullptr) {
        return;
    }

    const std::size_t frameBufferLength = static_cast<std::size_t>(frame[0]) + 1U;
    const std::uint8_t *payload = nullptr;
    std::size_t payloadLength = 0;
    issp154_mac_source_t replyContext{};
    if (issp154_mac_extract_payload_and_source(
            frame,
            frameBufferLength,
            &payload,
            &payloadLength,
            &replyContext) &&
        payload != nullptr && payloadLength > 0 && receiveHandler_ != nullptr) {
        receiveHandler_(payload, payloadLength, &replyContext, receiveContext_);
    }

}

} // namespace issp
