#include "issp154_transport.hpp"

#include <algorithm>

#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "issp154_mac_frame.h"
#include "issp154_transport.h"
#include "issp_protocol.hpp"

namespace issp
{

namespace
{

constexpr std::uint32_t kPhysicalTxTimeoutMs = 100;
constexpr std::uint8_t kExtendedAddressMode = 3;
constexpr EventBits_t kAckOutcomeAvailableBit = BIT0;
constexpr EventBits_t kAckExpectationClearedBit = BIT1;
constexpr EventBits_t kDiscoveryOutcomeAvailableBit = BIT2;
constexpr EventBits_t kAckWaitBits =
    kAckOutcomeAvailableBit | kAckExpectationClearedBit;
constexpr std::array<std::uint32_t, 2> kConfirmedSendBackoffMs{5, 10};

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
      hasDestination_(false),
      ackExpectation_{},
      ackOutcome_{},
      ackExpectationActive_(false),
      ackOutcomeAvailable_(false),
      ackWaitActive_(false),
      discoveryDeviceId_(0),
      discoverySequence_(0),
      discoveredAddress_{},
      discoveryActive_(false),
      discoveryOutcomeAvailable_(false),
      discoveryOutcomeValid_(false),
      ackEventGroupStorage_{},
      ackEventGroup_(nullptr)
{
}

IsspResult Issp154Transport::setDestination(const std::uint8_t *extendedAddress,
                                            std::size_t length)
{
    if (extendedAddress == nullptr || length != destination_.size()) {
        return IsspResult::InvalidArgument;
    }

    portENTER_CRITICAL(&ackLock_);
    std::copy_n(extendedAddress, destination_.size(), destination_.begin());
    hasDestination_ = true;
    portEXIT_CRITICAL(&ackLock_);
    return IsspResult::Ok;
}

void Issp154Transport::clearDestination()
{
    portENTER_CRITICAL(&ackLock_);
    destination_.fill(0);
    hasDestination_ = false;
    portEXIT_CRITICAL(&ackLock_);
}

bool Issp154Transport::hasDestination() const
{
    portENTER_CRITICAL(&ackLock_);
    const bool result = hasDestination_;
    portEXIT_CRITICAL(&ackLock_);
    return result;
}

IsspResult Issp154Transport::armAckExpectation(
    const Issp154AckExpectation &expectation)
{
    if (ackEventGroup_ != nullptr) {
        xEventGroupClearBits(ackEventGroup_, kAckWaitBits);
    }

    portENTER_CRITICAL(&ackLock_);
    IsspResult result = IsspResult::Ok;
    if (!hasDestination_) {
        result = IsspResult::NotReady;
    } else if (ackExpectationActive_ || ackOutcomeAvailable_ || ackWaitActive_ ||
               discoveryActive_) {
        result = IsspResult::Busy;
    } else {
        ackExpectation_ = expectation;
        ackOutcome_ = {};
        ackExpectationActive_ = true;
        ackOutcomeAvailable_ = false;
    }
    portEXIT_CRITICAL(&ackLock_);
    return result;
}

void Issp154Transport::clearAckExpectation()
{
    portENTER_CRITICAL(&ackLock_);
    ackExpectation_ = {};
    ackOutcome_ = {};
    ackExpectationActive_ = false;
    ackOutcomeAvailable_ = false;
    portEXIT_CRITICAL(&ackLock_);

    if (ackEventGroup_ != nullptr) {
        xEventGroupClearBits(ackEventGroup_, kAckOutcomeAvailableBit);
        xEventGroupSetBits(ackEventGroup_, kAckExpectationClearedBit);
    }
}

bool Issp154Transport::takeAckAttemptOutcome(Issp154AckAttemptOutcome &outcome)
{
    portENTER_CRITICAL(&ackLock_);
    const bool available = ackOutcomeAvailable_;
    if (available) {
        outcome = ackOutcome_;
        ackExpectation_ = {};
        ackOutcome_ = {};
        ackExpectationActive_ = false;
        ackOutcomeAvailable_ = false;
    } else {
        outcome = {};
    }
    portEXIT_CRITICAL(&ackLock_);
    if (available && ackEventGroup_ != nullptr) {
        xEventGroupClearBits(ackEventGroup_, kAckOutcomeAvailableBit);
        xEventGroupSetBits(ackEventGroup_, kAckExpectationClearedBit);
    }
    return available;
}

bool Issp154Transport::hasPendingAckExpectation() const
{
    portENTER_CRITICAL(&ackLock_);
    const bool pending = ackExpectationActive_ && !ackOutcomeAvailable_;
    portEXIT_CRITICAL(&ackLock_);
    return pending;
}

IsspResult Issp154Transport::waitAckAttemptOutcome(
    std::uint32_t timeoutMs,
    Issp154AckAttemptOutcome &outcome)
{
    outcome = {};
    if (timeoutMs == 0) {
        return IsspResult::InvalidArgument;
    }
    if (ackEventGroup_ == nullptr) {
        return IsspResult::NotReady;
    }

    portENTER_CRITICAL(&ackLock_);
    if (!ackExpectationActive_) {
        portEXIT_CRITICAL(&ackLock_);
        return IsspResult::NotReady;
    }
    if (ackOutcomeAvailable_) {
        outcome = ackOutcome_;
        ackExpectation_ = {};
        ackOutcome_ = {};
        ackExpectationActive_ = false;
        ackOutcomeAvailable_ = false;
        portEXIT_CRITICAL(&ackLock_);
        xEventGroupClearBits(ackEventGroup_, kAckWaitBits);
        return IsspResult::Ok;
    }
    if (ackWaitActive_) {
        portEXIT_CRITICAL(&ackLock_);
        return IsspResult::Busy;
    }
    ackWaitActive_ = true;
    portEXIT_CRITICAL(&ackLock_);

    std::uint64_t waitTicks =
        (static_cast<std::uint64_t>(timeoutMs) * configTICK_RATE_HZ + 999U) /
        1000U;
    if (waitTicks == 0) {
        waitTicks = 1;
    }
    if (waitTicks > static_cast<std::uint64_t>(portMAX_DELAY)) {
        waitTicks = static_cast<std::uint64_t>(portMAX_DELAY);
    }

    (void)xEventGroupWaitBits(
        ackEventGroup_,
        kAckWaitBits,
        pdTRUE,
        pdFALSE,
        static_cast<TickType_t>(waitTicks));

    IsspResult result = IsspResult::Failed;
    portENTER_CRITICAL(&ackLock_);
    if (ackOutcomeAvailable_) {
        outcome = ackOutcome_;
        ackExpectation_ = {};
        ackOutcome_ = {};
        ackExpectationActive_ = false;
        ackOutcomeAvailable_ = false;
        result = IsspResult::Ok;
    } else if (!ackExpectationActive_) {
        result = IsspResult::NotReady;
    }
    ackWaitActive_ = false;
    portEXIT_CRITICAL(&ackLock_);
    return result;
}

IsspResult Issp154Transport::begin()
{
    if (ackEventGroup_ == nullptr) {
        ackEventGroup_ = xEventGroupCreateStatic(&ackEventGroupStorage_);
        if (ackEventGroup_ == nullptr) {
            state_ = IsspTransportState::Error;
            return IsspResult::Failed;
        }
    }
    clearAckExpectation();
    portENTER_CRITICAL(&ackLock_);
    discoveryDeviceId_ = 0;
    discoverySequence_ = 0;
    discoveredAddress_.fill(0);
    discoveryActive_ = false;
    discoveryOutcomeAvailable_ = false;
    discoveryOutcomeValid_ = false;
    portEXIT_CRITICAL(&ackLock_);

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

IsspResult Issp154Transport::end()
{
    if (state_ == IsspTransportState::Stopped) {
        return IsspResult::Ok;
    }

    clearAckExpectation();
    portENTER_CRITICAL(&ackLock_);
    discoveryDeviceId_ = 0;
    discoverySequence_ = 0;
    discoveredAddress_.fill(0);
    discoveryActive_ = false;
    discoveryOutcomeAvailable_ = false;
    discoveryOutcomeValid_ = false;
    portEXIT_CRITICAL(&ackLock_);

    const esp_err_t error = issp154_transport_deinit();
    if (error != ESP_OK) {
        state_ = IsspTransportState::Error;
        return mapTransmitError(error);
    }

    if (ackEventGroup_ != nullptr) {
        vEventGroupDelete(ackEventGroup_);
        ackEventGroup_ = nullptr;
    }
    state_ = IsspTransportState::Stopped;
    return IsspResult::Ok;
}

IsspResult Issp154Transport::discoverDestination(
    std::uint32_t deviceId,
    std::uint16_t sequence,
    std::array<std::uint8_t, kIssp154ExtendedAddressSize> &destination)
{
    destination.fill(0);
    if (state_ != IsspTransportState::Ready || ackEventGroup_ == nullptr) {
        return IsspResult::NotReady;
    }
    if (config_.extendedAddress == nullptr) {
        return IsspResult::InvalidArgument;
    }

    std::array<std::uint8_t, IsspPayloadSize> payload{};
    std::size_t payloadLength = 0;
    const IsspResult encodeResult = encodeDiscoveryRequest(
        deviceId, sequence, payload.data(), payload.size(), payloadLength);
    if (encodeResult != IsspResult::Ok) {
        return encodeResult;
    }

    portENTER_CRITICAL(&ackLock_);
    if (ackExpectationActive_ || ackOutcomeAvailable_ || ackWaitActive_ ||
        discoveryActive_) {
        portEXIT_CRITICAL(&ackLock_);
        return IsspResult::Busy;
    }
    discoveryDeviceId_ = deviceId;
    discoverySequence_ = sequence;
    discoveredAddress_.fill(0);
    discoveryActive_ = true;
    discoveryOutcomeAvailable_ = false;
    discoveryOutcomeValid_ = false;
    portEXIT_CRITICAL(&ackLock_);

    IsspResult result = IsspResult::NotReady;
    for (std::uint8_t attempt = 0; attempt < 3; ++attempt) {
        if (issp154_transport_is_synchronous_transmit_busy()) {
            result = IsspResult::Busy;
            break;
        }

        portENTER_CRITICAL(&ackLock_);
        discoveryOutcomeAvailable_ = false;
        discoveryOutcomeValid_ = false;
        discoveredAddress_.fill(0);
        portEXIT_CRITICAL(&ackLock_);
        xEventGroupClearBits(ackEventGroup_, kDiscoveryOutcomeAvailableBit);

        std::size_t frameLength = 0;
        const std::uint8_t macSequence = macSequence_;
        if (!issp154_mac_build_broadcast_from_extended(
                config_.panId,
                config_.extendedAddress,
                macSequence,
                payload.data(),
                payloadLength,
                txFrame_.data(),
                txFrame_.size(),
                &frameLength) ||
            frameLength != static_cast<std::size_t>(txFrame_[0]) + 1U) {
            result = IsspResult::Failed;
            break;
        }
        ++macSequence_;

        const IsspResult transmitResult = mapTransmitError(
            issp154_transport_transmit_and_wait(
                txFrame_.data(), config_.cca, kPhysicalTxTimeoutMs));
        if (transmitResult == IsspResult::Busy) {
            result = IsspResult::Busy;
            break;
        }
        if (transmitResult == IsspResult::InvalidArgument) {
            result = IsspResult::InvalidArgument;
            break;
        }
        if (transmitResult != IsspResult::Ok) {
            continue;
        }

        (void)xEventGroupWaitBits(
            ackEventGroup_,
            kDiscoveryOutcomeAvailableBit,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(120));

        portENTER_CRITICAL(&ackLock_);
        const bool valid = discoveryOutcomeAvailable_ && discoveryOutcomeValid_;
        if (valid) {
            destination = discoveredAddress_;
        }
        portEXIT_CRITICAL(&ackLock_);
        if (valid) {
            result = IsspResult::Ok;
            break;
        }
    }

    portENTER_CRITICAL(&ackLock_);
    discoveryDeviceId_ = 0;
    discoverySequence_ = 0;
    discoveredAddress_.fill(0);
    discoveryActive_ = false;
    discoveryOutcomeAvailable_ = false;
    discoveryOutcomeValid_ = false;
    portEXIT_CRITICAL(&ackLock_);
    xEventGroupClearBits(ackEventGroup_, kDiscoveryOutcomeAvailableBit);
    return result;
}

IsspResult Issp154Transport::send(const std::uint8_t *data, std::size_t length)
{
    return transmitPayloadOnce(data, length);
}

IsspResult Issp154Transport::sendConfirmedOnce(
    const std::uint8_t *data,
    std::size_t length,
    const Issp154AckExpectation &expectation,
    std::uint32_t ackTimeoutMs,
    Issp154ConfirmedSendResult &result)
{
    result = {};
    if (data == nullptr || length == 0 || ackTimeoutMs == 0) {
        return IsspResult::InvalidArgument;
    }
    if (state_ != IsspTransportState::Ready || !hasDestination()) {
        return IsspResult::NotReady;
    }
    if (issp154_transport_is_synchronous_transmit_busy()) {
        return IsspResult::Busy;
    }

    IsspResult operationResult = armAckExpectation(expectation);
    ESP_LOGI("REPORT_TX",
             "ack_expectation arm_result=%u sequence=%u endpoint=%u",
             static_cast<unsigned>(operationResult),
             static_cast<unsigned>(expectation.sequence),
             static_cast<unsigned>(expectation.endpointId));
    if (operationResult != IsspResult::Ok) {
        return operationResult;
    }

    operationResult = transmitPayloadOnce(data, length);
    ESP_LOGI("REPORT_TX",
             "physical_transmit result=%u sequence=%u",
             static_cast<unsigned>(operationResult),
             static_cast<unsigned>(expectation.sequence));
    if (operationResult != IsspResult::Ok) {
        clearAckExpectation();
        return operationResult;
    }

    Issp154AckAttemptOutcome outcome{};
    ESP_LOGI("REPORT_TX",
             "ack_wait begin sequence=%u timeout_ms=%lu",
             static_cast<unsigned>(expectation.sequence),
             static_cast<unsigned long>(ackTimeoutMs));
    operationResult = waitAckAttemptOutcome(ackTimeoutMs, outcome);
    ESP_LOGI("REPORT_TX",
             "ack_wait result=%u outcome=%u status=%u sequence=%u",
             static_cast<unsigned>(operationResult),
             static_cast<unsigned>(outcome.result),
             static_cast<unsigned>(outcome.ackStatus),
             static_cast<unsigned>(expectation.sequence));
    if (operationResult == IsspResult::Failed &&
        outcome.result == Issp154AckAttemptResult::None) {
        ESP_LOGI("REPORT_TX",
                 "ack_wait outcome=timeout sequence=%u timeout_ms=%lu",
                 static_cast<unsigned>(expectation.sequence),
                 static_cast<unsigned long>(ackTimeoutMs));
    } else if (operationResult == IsspResult::Ok &&
               outcome.result == Issp154AckAttemptResult::AckReceived) {
        ESP_LOGI("REPORT_TX",
                 "ack_wait outcome=ack_received sequence=%u status=%u",
                 static_cast<unsigned>(expectation.sequence),
                 static_cast<unsigned>(outcome.ackStatus));
    }
    if (operationResult != IsspResult::Ok) {
        clearAckExpectation();
        return operationResult;
    }

    result = {
        .attemptResult = outcome.result,
        .ackStatus = outcome.ackStatus,
    };
    return IsspResult::Ok;
}

IsspResult Issp154Transport::sendConfirmed(
    const std::uint8_t *data,
    std::size_t length,
    const Issp154AckExpectation &expectation,
    std::uint32_t ackTimeoutMs,
    Issp154ConfirmedSendSummary &summary)
{
    summary = {};
    ESP_LOGI("REPORT_TX",
             "send_confirmed begin sequence=%u destination=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x payload_len=%u timeout_ms=%lu",
             static_cast<unsigned>(expectation.sequence),
             destination_[0], destination_[1], destination_[2], destination_[3],
             destination_[4], destination_[5], destination_[6], destination_[7],
             static_cast<unsigned>(length),
             static_cast<unsigned long>(ackTimeoutMs));
    if (data == nullptr || length == 0 || ackTimeoutMs == 0) {
        ESP_LOGI("REPORT_TX", "send_confirmed result=%u reason=invalid_argument",
                 static_cast<unsigned>(IsspResult::InvalidArgument));
        return IsspResult::InvalidArgument;
    }

    for (std::uint8_t attempt = 0; attempt < 3; ++attempt) {
        if (attempt > 0) {
            if (hasPendingAckExpectation()) {
                return IsspResult::Busy;
            }
            TickType_t delayTicks = pdMS_TO_TICKS(
                kConfirmedSendBackoffMs[attempt - 1U]);
            if (delayTicks == 0) {
                delayTicks = 1;
            }
            vTaskDelay(delayTicks);
        }

        Issp154ConfirmedSendResult attemptResult{};
        ESP_LOGI("REPORT_TX",
                 "attempt begin number=%u sequence=%u",
                 static_cast<unsigned>(attempt + 1U),
                 static_cast<unsigned>(expectation.sequence));
        const IsspResult operationResult = sendConfirmedOnce(
            data, length, expectation, ackTimeoutMs, attemptResult);
        ESP_LOGI("REPORT_TX",
                 "attempt result number=%u operation=%u ack_result=%u ack_status=%u sequence=%u",
                 static_cast<unsigned>(attempt + 1U),
                 static_cast<unsigned>(operationResult),
                 static_cast<unsigned>(attemptResult.attemptResult),
                 static_cast<unsigned>(attemptResult.ackStatus),
                 static_cast<unsigned>(expectation.sequence));
        if (operationResult == IsspResult::InvalidArgument ||
            operationResult == IsspResult::NotReady ||
            operationResult == IsspResult::Busy) {
            ESP_LOGI("REPORT_TX", "send_confirmed result=%u attempts=%u",
                     static_cast<unsigned>(operationResult),
                     static_cast<unsigned>(attempt + 1U));
            return operationResult;
        }

        summary.attempts = static_cast<std::uint8_t>(attempt + 1U);
        if (operationResult == IsspResult::Ok) {
            summary.attemptResult = attemptResult.attemptResult;
            summary.ackStatus = attemptResult.ackStatus;
            if (attemptResult.attemptResult ==
                Issp154AckAttemptResult::AckReceived) {
                ESP_LOGI("REPORT_TX",
                         "send_confirmed result=%u attempts=%u ack_status=%u",
                         static_cast<unsigned>(IsspResult::Ok),
                         static_cast<unsigned>(summary.attempts),
                         static_cast<unsigned>(summary.ackStatus));
                return IsspResult::Ok;
            }
        } else {
            summary.attemptResult = Issp154AckAttemptResult::None;
            summary.ackStatus = IsspAckStatus::Ok;
        }
    }

    ESP_LOGI("REPORT_TX", "send_confirmed result=%u attempts=%u",
             static_cast<unsigned>(IsspResult::Failed),
             static_cast<unsigned>(summary.attempts));
    return IsspResult::Failed;
}

IsspResult Issp154Transport::transmitPayloadOnce(const std::uint8_t *data,
                                                 std::size_t length)
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
        ESP_LOGI("MAC_PARSE", "rejected reason=null_frame");
        return;
    }

    const std::size_t frameBufferLength = static_cast<std::size_t>(frame[0]) + 1U;
    const std::uint8_t *payload = nullptr;
    std::size_t payloadLength = 0;
    issp154_mac_source_t replyContext{};
    const bool extracted = issp154_mac_extract_payload_and_source(
        frame,
        frameBufferLength,
        &payload,
        &payloadLength,
        &replyContext);
    if (!extracted) {
        ESP_LOGI("MAC_PARSE",
                 "rejected reason=mac_extract_failed frame_len=%u",
                 static_cast<unsigned>(frameBufferLength));
        return;
    }
    if (payload == nullptr) {
        ESP_LOGI("MAC_PARSE", "rejected reason=null_payload");
        return;
    }
    if (payloadLength == 0) {
        ESP_LOGI("MAC_PARSE", "rejected reason=empty_payload");
        return;
    }

    ESP_LOGI("MAC_PARSE",
             "ok src_mode=%u src=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x payload_len=%u",
             static_cast<unsigned>(replyContext.source_address_mode),
             replyContext.source_address[0],
             replyContext.source_address[1],
             replyContext.source_address[2],
             replyContext.source_address[3],
             replyContext.source_address[4],
             replyContext.source_address[5],
             replyContext.source_address[6],
             replyContext.source_address[7],
             static_cast<unsigned>(payloadLength));
    ESP_LOGI("TRANSPORT_RX",
             "payload received len=%u type=%u handler_registered=%s",
             static_cast<unsigned>(payloadLength),
             payloadLength > 1 ? static_cast<unsigned>(payload[1]) : 0U,
             receiveHandler_ != nullptr ? "yes" : "no");

    {
        std::uint32_t discoveryDeviceId = 0;
        std::uint16_t discoverySequence = 0;
        bool discoveryActive = false;
        portENTER_CRITICAL(&ackLock_);
        discoveryActive = discoveryActive_;
        if (discoveryActive) {
            discoveryDeviceId = discoveryDeviceId_;
            discoverySequence = discoverySequence_;
        }
        portEXIT_CRITICAL(&ackLock_);

        if (discoveryActive) {
            IsspDecodedDiscoveryResponse response{};
            const bool responseDecoded =
                decodeDiscoveryResponse(payload, payloadLength, response) ==
                IsspResult::Ok;
            const bool matches = responseDecoded &&
                replyContext.source_address_mode == kExtendedAddressMode &&
                replyContext.destination_address_mode == kExtendedAddressMode &&
                replyContext.source_pan_id == config_.panId &&
                replyContext.destination_pan_id == config_.panId &&
                std::equal(config_.extendedAddress,
                           config_.extendedAddress + kIssp154ExtendedAddressSize,
                           replyContext.destination_address) &&
                response.deviceId == discoveryDeviceId &&
                response.sequence == discoverySequence &&
                response.endpointId == 0 &&
                response.status == IsspAckStatus::Ok;

            bool recorded = false;
            portENTER_CRITICAL(&ackLock_);
            if (discoveryActive_ &&
                discoveryDeviceId_ == discoveryDeviceId &&
                discoverySequence_ == discoverySequence &&
                !discoveryOutcomeAvailable_) {
                discoveryOutcomeAvailable_ = true;
                discoveryOutcomeValid_ = matches;
                if (matches) {
                    std::copy_n(replyContext.source_address,
                                discoveredAddress_.size(),
                                discoveredAddress_.begin());
                }
                recorded = true;
            }
            portEXIT_CRITICAL(&ackLock_);
            if (recorded && ackEventGroup_ != nullptr) {
                xEventGroupSetBits(ackEventGroup_,
                                   kDiscoveryOutcomeAvailableBit);
            }
            return;
        }

        Issp154AckExpectation expected{};
        std::array<std::uint8_t, kIssp154ExtendedAddressSize> expectedSource{};
        std::uint16_t expectedPanId = 0;
        bool expectationActive = false;
        bool outcomeAvailable = false;
        bool destinationConfigured = false;
        bool outcomeRecorded = false;

        portENTER_CRITICAL(&ackLock_);
        expectationActive = ackExpectationActive_;
        if (expectationActive) {
            expected = ackExpectation_;
            outcomeAvailable = ackOutcomeAvailable_;
            expectedSource = destination_;
            expectedPanId = config_.panId;
            destinationConfigured = hasDestination_;
        }
        portEXIT_CRITICAL(&ackLock_);

        if (expectationActive) {
            IsspDecodedAck decodedAck{};
            const IsspResult decodeResult =
                decodeAck(payload, payloadLength, decodedAck);
            if (decodeResult == IsspResult::Ok) {
                const bool sourceMatches = destinationConfigured &&
                    replyContext.source_address_mode == kExtendedAddressMode &&
                    replyContext.source_pan_id == expectedPanId &&
                    std::equal(expectedSource.begin(), expectedSource.end(),
                               replyContext.source_address);
                const bool matches = sourceMatches &&
                    decodedAck.deviceId == expected.deviceId &&
                    decodedAck.sequence == expected.sequence &&
                    decodedAck.endpointId == expected.endpointId;
                ESP_LOGI("REPORT_ACK",
                         "received type=%u sequence=%u status=%u expected_sequence=%u match=%s",
                         payloadLength > 1 ? static_cast<unsigned>(payload[1]) : 0U,
                         static_cast<unsigned>(decodedAck.sequence),
                         static_cast<unsigned>(decodedAck.status),
                         static_cast<unsigned>(expected.sequence),
                         matches ? "yes" : "no");
                if (!outcomeAvailable) {
                    portENTER_CRITICAL(&ackLock_);
                    if (ackExpectationActive_ &&
                        ackExpectation_.deviceId == expected.deviceId &&
                        ackExpectation_.sequence == expected.sequence &&
                        ackExpectation_.endpointId == expected.endpointId &&
                        !ackOutcomeAvailable_) {
                        if (matches) {
                            ackOutcome_ = {
                                .result = Issp154AckAttemptResult::AckReceived,
                                .ackStatus = decodedAck.status,
                            };
                        } else {
                            ackOutcome_ = {
                                .result = Issp154AckAttemptResult::Interrupted,
                                .ackStatus = IsspAckStatus::Ok,
                            };
                        }
                        ackOutcomeAvailable_ = true;
                        outcomeRecorded = true;
                    }
                    portEXIT_CRITICAL(&ackLock_);
                }
                if (outcomeRecorded && ackEventGroup_ != nullptr) {
                    xEventGroupSetBits(
                        ackEventGroup_, kAckOutcomeAvailableBit);
                }
                return;
            }

            ESP_LOGI("REPORT_ACK",
                     "received type=%u decode_result=%u expected_sequence=%u match=no",
                     payloadLength > 1 ? static_cast<unsigned>(payload[1]) : 0U,
                     static_cast<unsigned>(decodeResult),
                     static_cast<unsigned>(expected.sequence));

            if (!outcomeAvailable) {
                portENTER_CRITICAL(&ackLock_);
                if (ackExpectationActive_ &&
                    ackExpectation_.deviceId == expected.deviceId &&
                    ackExpectation_.sequence == expected.sequence &&
                    ackExpectation_.endpointId == expected.endpointId &&
                    !ackOutcomeAvailable_) {
                    ackOutcome_ = {
                        .result = Issp154AckAttemptResult::Interrupted,
                        .ackStatus = IsspAckStatus::Ok,
                    };
                    ackOutcomeAvailable_ = true;
                    outcomeRecorded = true;
                }
                portEXIT_CRITICAL(&ackLock_);
            }
        }

        if (outcomeRecorded && ackEventGroup_ != nullptr) {
            xEventGroupSetBits(ackEventGroup_, kAckOutcomeAvailableBit);
        }

        if (receiveHandler_ != nullptr) {
            ESP_LOGI("TRANSPORT_RX",
                     "delivering payload len=%u handler_registered=yes",
                     static_cast<unsigned>(payloadLength));
            receiveHandler_(payload, payloadLength, &replyContext, receiveContext_);
        } else {
            ESP_LOGI("TRANSPORT_RX",
                     "payload not delivered len=%u reason=no_handler",
                     static_cast<unsigned>(payloadLength));
        }
    }

}

} // namespace issp
