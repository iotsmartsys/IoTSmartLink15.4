#include "issp154_transport.hpp"

#include "esp_attr.h"
#include "issp154_transport.h"

namespace issp
{

namespace
{

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

} // namespace

Issp154Transport::Issp154Transport(const Issp154TransportConfig &config)
    : config_(config),
      receiveHandler_(nullptr),
      receiveContext_(nullptr),
      state_(IsspTransportState::Stopped)
{
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

    if (data[0] == 0 || length != static_cast<std::size_t>(data[0]) + 1U) {
        return IsspResult::InvalidArgument;
    }

    const esp_err_t sleepError = issp154_transport_sleep();
    if (sleepError != ESP_OK) {
        return mapEspError(sleepError);
    }

    return mapEspError(issp154_transport_send(data, config_.cca));
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
        if (frame != nullptr) {
            issp154_transport_release_receive_buffer(frame);
        }
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
    constexpr std::uint8_t maxPhysicalFrameLength = 127;

    if (frame == nullptr) {
        return;
    }

    const std::uint8_t physicalLength = frame[0];
    if (physicalLength != 0 && physicalLength <= maxPhysicalFrameLength && receiveHandler_ != nullptr) {
        receiveHandler_(frame, static_cast<std::size_t>(physicalLength) + 1U, receiveContext_);
    }

    issp154_transport_release_receive_buffer(frame);
}

} // namespace issp
