#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
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

struct Issp154AckExpectation
{
    std::uint32_t deviceId;
    std::uint16_t sequence;
    std::uint8_t endpointId;
};

enum class Issp154AckAttemptResult : std::uint8_t
{
    None,
    AckReceived,
    Interrupted,
};

struct Issp154AckAttemptOutcome
{
    Issp154AckAttemptResult result;
    IsspAckStatus ackStatus;
};

struct Issp154ConfirmedSendResult
{
    Issp154AckAttemptResult attemptResult;
    IsspAckStatus ackStatus;
};

struct Issp154ConfirmedSendSummary
{
    Issp154AckAttemptResult attemptResult;
    IsspAckStatus ackStatus;
    std::uint8_t attempts;
};

class Issp154Transport final : public IIsspTransport
{
public:
    explicit Issp154Transport(const Issp154TransportConfig &config);

    IsspResult setDestination(const std::uint8_t *extendedAddress,
                              std::size_t length);
    void clearDestination();
    bool hasDestination() const;
    /// Performs up to three legacy discovery attempts. The caller must not be
    /// the RX task or an ISR.
    IsspResult discoverDestination(
        std::uint32_t deviceId,
        std::uint16_t sequence,
        std::array<std::uint8_t, kIssp154ExtendedAddressSize> &destination);

    IsspResult armAckExpectation(const Issp154AckExpectation &expectation);
    void clearAckExpectation();
    bool takeAckAttemptOutcome(Issp154AckAttemptOutcome &outcome);
    bool hasPendingAckExpectation() const;
    /// Blocks the caller, which must not be the RX task or an ISR.
    IsspResult waitAckAttemptOutcome(std::uint32_t timeoutMs,
                                     Issp154AckAttemptOutcome &outcome);
    /// Executes one confirmed attempt. The caller must not be the RX task or
    /// an ISR. Concurrent callers have no fairness guarantee.
    IsspResult sendConfirmedOnce(
        const std::uint8_t *data,
        std::size_t length,
        const Issp154AckExpectation &expectation,
        std::uint32_t ackTimeoutMs,
        Issp154ConfirmedSendResult &result);
    /// Executes up to three confirmed attempts. The caller must not be the RX
    /// task or an ISR. Concurrent callers have no fairness guarantee.
    IsspResult sendConfirmed(
        const std::uint8_t *data,
        std::size_t length,
        const Issp154AckExpectation &expectation,
        std::uint32_t ackTimeoutMs,
        Issp154ConfirmedSendSummary &summary);

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

    IsspResult transmitPayloadOnce(const std::uint8_t *data,
                                   std::size_t length);
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
    Issp154AckExpectation ackExpectation_;
    Issp154AckAttemptOutcome ackOutcome_;
    bool ackExpectationActive_;
    bool ackOutcomeAvailable_;
    bool ackWaitActive_;
    std::uint32_t discoveryDeviceId_;
    std::uint16_t discoverySequence_;
    std::array<std::uint8_t, kIssp154ExtendedAddressSize> discoveredAddress_;
    bool discoveryActive_;
    bool discoveryOutcomeAvailable_;
    bool discoveryOutcomeValid_;
    StaticEventGroup_t ackEventGroupStorage_;
    EventGroupHandle_t ackEventGroup_;
    mutable portMUX_TYPE ackLock_ = portMUX_INITIALIZER_UNLOCKED;
};

} // namespace issp
