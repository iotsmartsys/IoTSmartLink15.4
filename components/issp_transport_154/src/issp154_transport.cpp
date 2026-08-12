#include "issp154_transport.hpp"

#include <algorithm>

#include "esp_attr.h"
#include "esp_ieee802154.h"
#include "esp_log.h"
#include "esp_timer.h"
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
constexpr std::uint8_t kCommandMessageType = 5;
constexpr EventBits_t kAckOutcomeAvailableBit = BIT0;
constexpr EventBits_t kAckExpectationClearedBit = BIT1;
constexpr EventBits_t kDiscoveryOutcomeAvailableBit = BIT2;
constexpr EventBits_t kAckWaitBits =
    kAckOutcomeAvailableBit | kAckExpectationClearedBit;
constexpr std::array<std::uint32_t, 2> kConfirmedSendBackoffMs{5, 10};

std::uint16_t diagnosticSequence(const std::uint8_t *data, std::size_t length)
{
    if (data == nullptr || length < 8U) {
        return 0;
    }
    return static_cast<std::uint16_t>(data[6]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[7]) << 8U);
}

std::uint32_t diagnosticDeviceId(const std::uint8_t *data, std::size_t length)
{
    if (data == nullptr || length < 6U) {
        return 0;
    }
    return static_cast<std::uint32_t>(data[2]) |
           (static_cast<std::uint32_t>(data[3]) << 8U) |
           (static_cast<std::uint32_t>(data[4]) << 16U) |
           (static_cast<std::uint32_t>(data[5]) << 24U);
}

const char *messageTypeName(std::uint8_t messageType)
{
    switch (messageType) {
    case 1:
        return "report";
    case 2:
        return "ack";
    case 3:
        return "discovery_request";
    case 4:
        return "discovery_response";
    case 5:
        return "command";
    default:
        return "unknown";
    }
}

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
      discoveredPanId_(0),
      discoveryActive_(false),
      discoveryOutcomeAvailable_(false),
      discoveryOutcomeValid_(false),
      ackEventGroupStorage_{},
      ackEventGroup_(nullptr)
{
}

IsspResult Issp154Transport::configureNetwork(std::uint8_t channel,
                                              std::uint16_t panId,
                                              bool promiscuous)
{
    if (state_ != IsspTransportState::Stopped || channel < 11U || channel > 26U) {
        return state_ == IsspTransportState::Stopped
            ? IsspResult::InvalidArgument
            : IsspResult::Busy;
    }
    config_.channel = channel;
    config_.panId = panId;
    config_.promiscuous = promiscuous;
    clearDestination();
    return IsspResult::Ok;
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
    discoveredPanId_ = 0;
    discoveryActive_ = false;
    discoveryOutcomeAvailable_ = false;
    discoveryOutcomeValid_ = false;
    portEXIT_CRITICAL(&ackLock_);

    if (config_.extendedAddress == nullptr) {
        state_ = IsspTransportState::Error;
        return IsspResult::InvalidArgument;
    }
    if (config_.channel < 11U || config_.channel > 26U) {
        state_ = IsspTransportState::Error;
        return IsspResult::InvalidArgument;
    }

    state_ = IsspTransportState::Starting;

    const issp154_transport_config_t transportConfig = {
        .channel = config_.channel,
        .pan_id = config_.panId,
        .short_address = config_.shortAddress,
        .coordinator = config_.coordinator,
        .promiscuous = config_.promiscuous,
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
    discoveredPanId_ = 0;
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

IsspResult Issp154Transport::discoverNetwork(
    std::uint32_t deviceId,
    std::uint16_t sequence,
    Issp154DiscoveredNetwork &network)
{
    network = {};
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
    discoveredPanId_ = 0;
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
        discoveredPanId_ = 0;
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
            network.coordinatorAddress = discoveredAddress_;
            network.panId = discoveredPanId_;
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
    discoveredPanId_ = 0;
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
    const int64_t attemptStartedAtUs = esp_timer_get_time();
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
    if (operationResult != IsspResult::Ok) {
        return operationResult;
    }
    ESP_LOGI("ISSP_PROTOCOL_TRACE",
             "event=ack_expectation_armed role=report device=0x%08lx issp_seq=%u report_id=%016llX endpoint=%u timeout_ms=%lu",
             static_cast<unsigned long>(expectation.deviceId),
             static_cast<unsigned>(expectation.sequence),
             static_cast<unsigned long long>(expectation.reportId),
             static_cast<unsigned>(expectation.endpointId),
             static_cast<unsigned long>(ackTimeoutMs));

    operationResult = transmitPayloadOnce(data, length);
    if (operationResult != IsspResult::Ok) {
        clearAckExpectation();
        return operationResult;
    }

    Issp154AckAttemptOutcome outcome{};
    operationResult = waitAckAttemptOutcome(ackTimeoutMs, outcome);
    if (operationResult == IsspResult::Failed &&
        outcome.result == Issp154AckAttemptResult::None) {
        ESP_LOGW("ISSP_TRANSPORT",
                 "report ACK timeout sequence=%u elapsed_us=%lld radio_state=%u rx_drops=%lu",
                 static_cast<unsigned>(expectation.sequence),
                 static_cast<long long>(esp_timer_get_time() - attemptStartedAtUs),
                 static_cast<unsigned>(esp_ieee802154_get_state()),
                 static_cast<unsigned long>(issp154_transport_rx_drop_count()));
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
    if (data == nullptr || length == 0 || ackTimeoutMs == 0) {
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
        const IsspResult operationResult = sendConfirmedOnce(
            data, length, expectation, ackTimeoutMs, attemptResult);
        if (operationResult == IsspResult::InvalidArgument ||
            operationResult == IsspResult::NotReady ||
            operationResult == IsspResult::Busy) {
            return operationResult;
        }

        summary.attempts = static_cast<std::uint8_t>(attempt + 1U);
        if (operationResult == IsspResult::Ok) {
            summary.attemptResult = attemptResult.attemptResult;
            summary.ackStatus = attemptResult.ackStatus;
            if (attemptResult.attemptResult ==
                Issp154AckAttemptResult::AckReceived) {
                return IsspResult::Ok;
            }
        } else {
            summary.attemptResult = Issp154AckAttemptResult::None;
            summary.ackStatus = IsspAckStatus::Ok;
        }
    }

    ESP_LOGW("ISSP_TRANSPORT", "confirmed report failed sequence=%u attempts=%u",
             static_cast<unsigned>(expectation.sequence),
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

    const std::uint8_t messageType = length > 1U ? data[1] : 0U;
    ESP_LOGI("ISSP_PROTOCOL_TRACE",
             "event=tx_prepare role=%s device=0x%08lx issp_seq=%u endpoint=%u payload_len=%u mac_seq=%u",
             messageTypeName(messageType),
             static_cast<unsigned long>(diagnosticDeviceId(data, length)),
             static_cast<unsigned>(diagnosticSequence(data, length)),
             static_cast<unsigned>(length > 16U ? data[16] : 0U),
             static_cast<unsigned>(length),
             static_cast<unsigned>(macSequence_));

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
        ESP_LOGW("ISSP_PROTOCOL_TRACE",
                 "event=tx_reply_rejected reason=transport_busy role=%s device=0x%08lx issp_seq=%u endpoint=%u",
                 messageTypeName(length > 1U ? data[1] : 0U),
                 static_cast<unsigned long>(diagnosticDeviceId(data, length)),
                 static_cast<unsigned>(diagnosticSequence(data, length)),
                 static_cast<unsigned>(length > 16U ? data[16] : 0U));
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
    ESP_LOGI("ISSP_PROTOCOL_TRACE",
             "event=tx_prepare role=command_ack device=0x%08lx issp_seq=%u endpoint=%u payload_len=%u mac_seq=%u",
             static_cast<unsigned long>(diagnosticDeviceId(data, length)),
             static_cast<unsigned>(diagnosticSequence(data, length)),
             static_cast<unsigned>(length > 16U ? data[16] : 0U),
             static_cast<unsigned>(length),
             static_cast<unsigned>(sequence));
    const IsspResult result = mapTransmitError(issp154_transport_transmit_and_wait(
        replyFrame_.data(), config_.cca, kPhysicalTxTimeoutMs));
    ESP_LOGI("ISSP_PROTOCOL_TRACE",
             "event=tx_result role=command_ack device=0x%08lx issp_seq=%u endpoint=%u result=%u",
             static_cast<unsigned long>(diagnosticDeviceId(data, length)),
             static_cast<unsigned>(diagnosticSequence(data, length)),
             static_cast<unsigned>(length > 16U ? data[16] : 0U),
             static_cast<unsigned>(result));
    return result;
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
    instance->notifyReceive(frame, frameInfo);
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

void IRAM_ATTR Issp154Transport::notifyReceive(
    std::uint8_t *frame,
    const esp_ieee802154_frame_info_t *frameInfo)
{
    if (frame == nullptr) {
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
        return;
    }
    if (payload == nullptr) {
        return;
    }
    if (payloadLength == 0) {
        return;
    }

    if (payloadLength != IsspPayloadSize) {
        // The codecs reject by length before decoding, so a v1 frame reaching a
        // v2 endpoint is only distinguishable by inspecting byte 0 directly.
        // Nothing beyond it is decoded.
        ESP_LOGW("ISSP_PROTOCOL_TRACE",
                 "event=rx_rejected reason=payload_length wire_version=%u payload_len=%u expected_len=%u",
                 static_cast<unsigned>(payload[0]),
                 static_cast<unsigned>(payloadLength),
                 static_cast<unsigned>(IsspPayloadSize));
        return;
    }

    const std::uint8_t receivedMessageType = payloadLength > 1U ? payload[1] : 0U;
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
                replyContext.source_pan_id != 0xffffU &&
                replyContext.destination_pan_id == replyContext.source_pan_id &&
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
                    discoveredPanId_ = replyContext.source_pan_id;
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
                // A report ACK only concludes the attempt when the identity
                // also matches: a late ACK of another sequence or identity, and
                // a command ACK (report_id == 0), never conclude a report.
                const bool matches = sourceMatches &&
                    decodedAck.deviceId == expected.deviceId &&
                    decodedAck.sequence == expected.sequence &&
                    decodedAck.reportId != 0U &&
                    decodedAck.reportId == expected.reportId &&
                    decodedAck.endpointId == expected.endpointId;
                if (!outcomeAvailable) {
                    portENTER_CRITICAL(&ackLock_);
                    if (ackExpectationActive_ &&
                        ackExpectation_.deviceId == expected.deviceId &&
                        ackExpectation_.sequence == expected.sequence &&
                        ackExpectation_.reportId == expected.reportId &&
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
                if (!matches) {
                    ESP_LOGW("ISSP_TRANSPORT",
                             "incompatible report ACK expected_device=0x%08lx received_device=0x%08lx expected_seq=%u received_seq=%u expected_report_id=%016llX received_report_id=%016llX expected_endpoint=%u received_endpoint=%u source_match=%s",
                             static_cast<unsigned long>(expected.deviceId),
                             static_cast<unsigned long>(decodedAck.deviceId),
                             static_cast<unsigned>(expected.sequence),
                             static_cast<unsigned>(decodedAck.sequence),
                             static_cast<unsigned long long>(expected.reportId),
                             static_cast<unsigned long long>(decodedAck.reportId),
                             static_cast<unsigned>(expected.endpointId),
                             static_cast<unsigned>(decodedAck.endpointId),
                             sourceMatches ? "yes" : "no");
                } else {
                    ESP_LOGI("ISSP_PROTOCOL_TRACE",
                             "event=ack_matched role=report device=0x%08lx issp_seq=%u report_id=%016llX endpoint=%u status=%u rssi=%d lqi=%u sfd_us=%llu",
                             static_cast<unsigned long>(decodedAck.deviceId),
                             static_cast<unsigned>(decodedAck.sequence),
                             static_cast<unsigned long long>(decodedAck.reportId),
                             static_cast<unsigned>(decodedAck.endpointId),
                             static_cast<unsigned>(decodedAck.status),
                             frameInfo != nullptr ? static_cast<int>(frameInfo->rssi) : 0,
                             frameInfo != nullptr ? static_cast<unsigned>(frameInfo->lqi) : 0U,
                             frameInfo != nullptr
                                 ? static_cast<unsigned long long>(frameInfo->timestamp)
                                 : 0ULL);
                }
                return;
            }

            if (!outcomeAvailable) {
                portENTER_CRITICAL(&ackLock_);
                if (ackExpectationActive_ &&
                    ackExpectation_.deviceId == expected.deviceId &&
                    ackExpectation_.sequence == expected.sequence &&
                    ackExpectation_.reportId == expected.reportId &&
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
        if (outcomeRecorded) {
            ESP_LOGW("ISSP_PROTOCOL_TRACE",
                     "event=ack_expectation_interrupted expected_seq=%u received_role=%s received_seq=%u",
                     static_cast<unsigned>(expected.sequence),
                     messageTypeName(receivedMessageType),
                     static_cast<unsigned>(diagnosticSequence(payload, payloadLength)));
        }

        if (payloadLength > 1 && payload[1] == kCommandMessageType) {
            const bool destinationConfigured = hasDestination_;
            const bool sourceModeMatches =
                replyContext.source_address_mode == kExtendedAddressMode;
            const bool destinationModeMatches =
                replyContext.destination_address_mode == kExtendedAddressMode;
            const bool sourcePanMatches =
                replyContext.source_pan_id == config_.panId;
            const bool destinationPanMatches =
                replyContext.destination_pan_id == config_.panId;
            const bool sourceAddressMatches = destinationConfigured &&
                std::equal(destination_.begin(), destination_.end(),
                           replyContext.source_address);
            const bool destinationAddressMatches =
                config_.extendedAddress != nullptr &&
                std::equal(config_.extendedAddress,
                           config_.extendedAddress +
                               kIssp154ExtendedAddressSize,
                           replyContext.destination_address);
            const bool trusted = destinationConfigured &&
                sourceModeMatches && destinationModeMatches &&
                sourcePanMatches && destinationPanMatches &&
                sourceAddressMatches && destinationAddressMatches;

            ESP_LOGI("COMMAND_ORIGIN",
                     "trusted=%s destination_configured=%s src_mode=%u dst_mode=%u src_pan=0x%04x dst_pan=0x%04x src_match=%s dst_match=%s",
                     trusted ? "yes" : "no",
                     destinationConfigured ? "yes" : "no",
                     static_cast<unsigned>(replyContext.source_address_mode),
                     static_cast<unsigned>(replyContext.destination_address_mode),
                     static_cast<unsigned>(replyContext.source_pan_id),
                     static_cast<unsigned>(replyContext.destination_pan_id),
                     sourceAddressMatches ? "yes" : "no",
                     destinationAddressMatches ? "yes" : "no");

            if (!trusted) {
                ESP_LOGW("COMMAND_ORIGIN",
                         "command discarded reason=untrusted_mac_origin");
                return;
            }
        }

        if (receiveHandler_ != nullptr) {
            receiveHandler_(payload, payloadLength, &replyContext, receiveContext_);
        }
    }

}

} // namespace issp
