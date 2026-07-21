#include "issp_device.hpp"
#include "idevice_behavior.hpp"
#include "issp_protocol.hpp"

#include <cstdio>

namespace issp
{
    namespace
    {
        constexpr std::size_t kDiagnosticMessageTypeOffset = 1;
        constexpr std::size_t kDiagnosticDeviceIdOffset = 2;
        constexpr std::size_t kDiagnosticSequenceOffset = 6;
        constexpr std::size_t kDiagnosticEndpointOffset = 8;
        constexpr std::size_t kDiagnosticEventOffset = 9;
        constexpr std::size_t kDiagnosticValueOffset = 10;
        constexpr std::uint8_t kDiagnosticCommandMessageType = 5;

        std::uint32_t diagnosticReadUint32(const std::uint8_t *data)
        {
            return static_cast<std::uint32_t>(data[0]) |
                   (static_cast<std::uint32_t>(data[1]) << 8U) |
                   (static_cast<std::uint32_t>(data[2]) << 16U) |
                   (static_cast<std::uint32_t>(data[3]) << 24U);
        }

        std::uint16_t diagnosticReadUint16(const std::uint8_t *data)
        {
            return static_cast<std::uint16_t>(data[0]) |
                   static_cast<std::uint16_t>(
                       static_cast<std::uint16_t>(data[1]) << 8U);
        }
    }

    IsspDevice::IsspDevice(const IsspDeviceConfig &config, IIsspTransport &transport)
        : config_(config),
          transport_(transport),
          behaviors_{},
          behaviorCount_(0),
          commandHandler_(nullptr),
          commandContext_(nullptr),
          pendingReportHandler_(nullptr),
          pendingReportContext_(nullptr),
          processingCommand_(false),
          reportNotificationDeferred_(false),
          hasLastCommand_(false),
          lastCommandSequence_(0),
          lastCommand_{},
          lastCommandResult_(IsspCommandResult::Failed),
          reportSequence_(0),
          pendingReports_{},
          pendingReportCount_(0),
          nextPendingReportOrder_(0)
    {
    }

    IsspResult IsspDevice::addBehavior(IDeviceBehavior &behavior)
    {
        if (behaviorCount_ >= kMaxDeviceBehaviors)
        {
            return IsspResult::Failed;
        }

        behaviors_[behaviorCount_] = &behavior;
        ++behaviorCount_;
        return IsspResult::Ok;
    }

    IsspResult IsspDevice::start()
    {
        if (transport_.state() != IsspTransportState::Ready)
        {
            return IsspResult::NotReady;
        }

        for (std::size_t index = 0; index < behaviorCount_; ++index)
        {
            IDeviceBehavior *behavior = behaviors_[index];
            if (behavior == nullptr)
            {
                return IsspResult::Failed;
            }

            const IsspResult result = behavior->begin(*this);
            if (result != IsspResult::Ok)
            {
                return result;
            }
        }

        transport_.setReceiveHandler(&IsspDevice::handleReceive, this);
        return IsspResult::Ok;
    }

    std::uint32_t IsspDevice::deviceId() const
    {
        return config_.deviceId;
    }

    IsspTransportState IsspDevice::transportState() const
    {
        return transport_.state();
    }

    IsspResult IsspDevice::publishState(const IsspReport &report)
    {
        std::printf("REPORT_PENDING: publish endpoint=%u event=%u value=%u\n",
                    static_cast<unsigned>(report.endpointId),
                    static_cast<unsigned>(report.eventType),
                    static_cast<unsigned>(report.value));
        for (std::size_t index = 0; index < pendingReports_.size(); ++index)
        {
            PendingReportSlot &slot = pendingReports_[index];
            if (slot.occupied &&
                slot.report.endpointId == report.endpointId &&
                slot.report.eventType == report.eventType)
            {
                slot.report = report;
                advanceGeneration(slot.generation);
                std::printf("REPORT_PENDING: result=updated slot=%u generation=%lu\n",
                            static_cast<unsigned>(index),
                            static_cast<unsigned long>(slot.generation));
                if (processingCommand_)
                {
                    reportNotificationDeferred_ = true;
                    std::printf("REPORT_PENDING: notifying_executor=deferred slot=%u generation=%lu\n",
                                static_cast<unsigned>(index),
                                static_cast<unsigned long>(slot.generation));
                }
                else
                {
                    std::printf("REPORT_PENDING: notifying_executor=immediate slot=%u generation=%lu\n",
                                static_cast<unsigned>(index),
                                static_cast<unsigned long>(slot.generation));
                    notifyPendingReport();
                }
                return IsspResult::Ok;
            }
        }

        for (std::size_t index = 0; index < pendingReports_.size(); ++index)
        {
            PendingReportSlot &slot = pendingReports_[index];
            if (!slot.occupied)
            {
                slot.report = report;
                advanceGeneration(slot.generation);
                slot.inFlightGeneration = 0;
                slot.insertionOrder = nextPendingReportOrder();
                slot.occupied = true;
                slot.inFlight = false;
                ++pendingReportCount_;
                std::printf("REPORT_PENDING: result=created slot=%u generation=%lu\n",
                            static_cast<unsigned>(index),
                            static_cast<unsigned long>(slot.generation));
                if (processingCommand_)
                {
                    reportNotificationDeferred_ = true;
                    std::printf("REPORT_PENDING: notifying_executor=deferred slot=%u generation=%lu\n",
                                static_cast<unsigned>(index),
                                static_cast<unsigned long>(slot.generation));
                }
                else
                {
                    std::printf("REPORT_PENDING: notifying_executor=immediate slot=%u generation=%lu\n",
                                static_cast<unsigned>(index),
                                static_cast<unsigned long>(slot.generation));
                    notifyPendingReport();
                }
                return IsspResult::Ok;
            }
        }

        std::printf("REPORT_PENDING: result=failed reason=no_available_slot\n");
        return IsspResult::Failed;
    }

    IsspResult IsspDevice::publishReport(const IsspReport &report)
    {
        std::uint8_t payload[IsspPayloadSize]{};
        std::size_t payloadLength = 0;
        const IsspResult encodeResult = encodeReport(
            config_.deviceId,
            reportSequence_,
            report,
            payload,
            sizeof(payload),
            payloadLength);
        if (encodeResult != IsspResult::Ok)
        {
            return encodeResult;
        }

        ++reportSequence_;
        return transport_.send(payload, payloadLength);
    }

    std::size_t IsspDevice::pendingReportCount() const
    {
        return pendingReportCount_;
    }

    bool IsspDevice::peekPendingReport(IsspReport &report) const
    {
        const PendingReportSlot *oldestSlot = nullptr;
        for (const PendingReportSlot &slot : pendingReports_)
        {
            if (slot.occupied && !slot.inFlight &&
                (oldestSlot == nullptr || slot.insertionOrder < oldestSlot->insertionOrder))
            {
                oldestSlot = &slot;
            }
        }

        if (oldestSlot == nullptr)
        {
            report = {};
            return false;
        }

        report = oldestSlot->report;
        return true;
    }

    bool IsspDevice::acquirePendingReport(IsspReport &report, IsspPendingReportToken &token)
    {
        std::size_t oldestIndex = kMaxPendingReports;
        for (std::size_t index = 0; index < pendingReports_.size(); ++index)
        {
            const PendingReportSlot &slot = pendingReports_[index];
            if (slot.occupied && !slot.inFlight &&
                (oldestIndex == kMaxPendingReports ||
                 slot.insertionOrder < pendingReports_[oldestIndex].insertionOrder))
            {
                oldestIndex = index;
            }
        }

        if (oldestIndex == kMaxPendingReports)
        {
            report = {};
            token = {};
            return false;
        }

        PendingReportSlot &slot = pendingReports_[oldestIndex];
        slot.inFlight = true;
        slot.inFlightGeneration = slot.generation;
        report = slot.report;
        token.slotIndex = static_cast<std::uint8_t>(oldestIndex);
        token.generation = slot.inFlightGeneration;
        return true;
    }

    bool IsspDevice::completePendingReport(const IsspPendingReportToken &token, bool delivered)
    {
        if (token.slotIndex >= kMaxPendingReports)
        {
            return false;
        }

        PendingReportSlot &slot = pendingReports_[token.slotIndex];
        if (!slot.occupied || !slot.inFlight ||
            slot.inFlightGeneration != token.generation)
        {
            return false;
        }

        slot.inFlight = false;
        slot.inFlightGeneration = 0;

        if (delivered && slot.generation == token.generation)
        {
            slot.report = {};
            slot.insertionOrder = 0;
            slot.occupied = false;
            --pendingReportCount_;
        }

        return true;
    }

    IsspResult IsspDevice::preparePendingReport(IsspPreparedReport &preparedReport)
    {
        preparedReport = {};

        IsspReport report{};
        IsspPendingReportToken token{};
        if (!acquirePendingReport(report, token))
        {
            return IsspResult::NotReady;
        }

        const std::uint16_t sequence = reportSequence_;
        std::array<std::uint8_t, IsspPayloadSize> payload{};
        std::size_t payloadLength = 0;
        const IsspResult encodeResult = encodeReport(
            config_.deviceId,
            sequence,
            report,
            payload.data(),
            payload.size(),
            payloadLength);
        if (encodeResult != IsspResult::Ok)
        {
            (void)completePendingReport(token, false);
            return encodeResult;
        }

        ++reportSequence_;
        preparedReport.token = token;
        preparedReport.deviceId = config_.deviceId;
        preparedReport.sequence = sequence;
        preparedReport.report = report;
        preparedReport.payload = payload;
        preparedReport.payloadLength = payloadLength;
        return IsspResult::Ok;
    }

    void IsspDevice::setPendingReportHandler(
        PendingReportHandler handler,
        void *context)
    {
        pendingReportHandler_ = handler;
        pendingReportContext_ = context;
    }

    void IsspDevice::advanceGeneration(std::uint32_t &generation)
    {
        ++generation;
        if (generation == 0)
        {
            generation = 1;
        }
    }

    void IsspDevice::notifyPendingReport()
    {
        if (pendingReportHandler_ != nullptr)
        {
            pendingReportHandler_(pendingReportContext_);
        }
    }

    void IsspDevice::finishCommandProcessing()
    {
        const bool notifyDeferredReport = reportNotificationDeferred_;
        processingCommand_ = false;
        reportNotificationDeferred_ = false;
        if (notifyDeferredReport)
        {
            std::printf("REPORT_PENDING: notifying_executor=after_command_reply\n");
            notifyPendingReport();
        }
    }

    std::uint32_t IsspDevice::nextPendingReportOrder()
    {
        if (nextPendingReportOrder_ == UINT32_MAX)
        {
            normalizePendingReportOrders();
        }

        ++nextPendingReportOrder_;
        return nextPendingReportOrder_;
    }

    void IsspDevice::normalizePendingReportOrders()
    {
        std::uint32_t normalizedOrder = 0;
        std::uint32_t previousOrder = 0;

        for (std::size_t count = 0; count < pendingReportCount_; ++count)
        {
            PendingReportSlot *nextSlot = nullptr;
            for (PendingReportSlot &slot : pendingReports_)
            {
                if (slot.occupied && slot.insertionOrder > previousOrder &&
                    (nextSlot == nullptr || slot.insertionOrder < nextSlot->insertionOrder))
                {
                    nextSlot = &slot;
                }
            }

            if (nextSlot == nullptr)
            {
                break;
            }

            previousOrder = nextSlot->insertionOrder;
            ++normalizedOrder;
            nextSlot->insertionOrder = normalizedOrder;
        }

        nextPendingReportOrder_ = normalizedOrder;
    }

    void IsspDevice::setCommandHandler(CommandHandler handler, void *context)
    {
        commandHandler_ = handler;
        commandContext_ = context;
    }

    void IsspDevice::handleReceive(const std::uint8_t *data,
                                   std::size_t length,
                                   const void *replyContext,
                                   void *context)
    {
        std::printf("DEVICE_RX: payload received len=%u\n",
                    static_cast<unsigned>(length));
        if (context == nullptr)
        {
            return;
        }

        auto *device = static_cast<IsspDevice *>(context);
        device->onReceive(data, length, replyContext);
    }

    void IsspDevice::onReceive(const std::uint8_t *data,
                               std::size_t length,
                               const void *replyContext)
    {
        const unsigned messageType =
            data != nullptr && length > kDiagnosticMessageTypeOffset
                ? static_cast<unsigned>(data[kDiagnosticMessageTypeOffset])
                : 0U;
        std::printf("DEVICE_DISPATCH: enter len=%u message_type=%u\n",
                    static_cast<unsigned>(length),
                    messageType);
        std::printf("DEVICE_DISPATCH: forward_to_command_decode=yes\n");

        IsspDecodedCommand decodedCommand{};
        const IsspResult decodeResult =
            decodeCommand(data, length, config_.deviceId, decodedCommand);
        if (decodeResult != IsspResult::Ok)
        {
            if (data == nullptr)
            {
                std::printf("COMMAND_DECODE: result=%u reason=null_payload\n",
                            static_cast<unsigned>(decodeResult));
            }
            else if (length != IsspPayloadSize)
            {
                std::printf("COMMAND_DECODE: result=%u reason=invalid_length len=%u expected=%u\n",
                            static_cast<unsigned>(decodeResult),
                            static_cast<unsigned>(length),
                            static_cast<unsigned>(IsspPayloadSize));
            }
            else
            {
                const std::uint32_t receivedDeviceId =
                    diagnosticReadUint32(&data[kDiagnosticDeviceIdOffset]);
                const char *reason = "protocol_validation_failed";
                if (messageType != kDiagnosticCommandMessageType)
                {
                    reason = "unexpected_message_type";
                }
                else if (receivedDeviceId != config_.deviceId)
                {
                    reason = "device_id_mismatch";
                }
                std::printf("COMMAND_DECODE: result=%u reason=%s type=%u device_id=%lu expected_device_id=%lu sequence=%u endpoint=%u event=%u value=%u\n",
                            static_cast<unsigned>(decodeResult),
                            reason,
                            messageType,
                            static_cast<unsigned long>(receivedDeviceId),
                            static_cast<unsigned long>(config_.deviceId),
                            static_cast<unsigned>(diagnosticReadUint16(
                                &data[kDiagnosticSequenceOffset])),
                            static_cast<unsigned>(data[kDiagnosticEndpointOffset]),
                            static_cast<unsigned>(data[kDiagnosticEventOffset]),
                            static_cast<unsigned>(data[kDiagnosticValueOffset]));
            }
            std::printf("DEVICE_DISPATCH: discarded reason=command_decode_rejected\n");
            return;
        }

        std::printf("COMMAND_DECODE: result=%u sequence=%u device_id=%lu endpoint=%u event=%u value=%u\n",
                    static_cast<unsigned>(decodeResult),
                    static_cast<unsigned>(decodedCommand.sequence),
                    static_cast<unsigned long>(config_.deviceId),
                    static_cast<unsigned>(decodedCommand.command.endpointId),
                    static_cast<unsigned>(decodedCommand.command.eventType),
                    static_cast<unsigned>(decodedCommand.command.value));
        std::printf("DEVICE_DISPATCH: command_decode=accepted forwarding_to_behavior=yes\n");

        const bool duplicate = hasLastCommand_ &&
            lastCommandSequence_ == decodedCommand.sequence &&
            lastCommand_.endpointId == decodedCommand.command.endpointId &&
            lastCommand_.eventType == decodedCommand.command.eventType &&
            lastCommand_.value == decodedCommand.command.value;

        IsspCommandResult result = lastCommandResult_;
        if (duplicate)
        {
            std::printf("COMMAND_DEDUP: duplicate=yes sequence=%u action=resend_ack\n",
                        static_cast<unsigned>(decodedCommand.sequence));
        }
        else
        {
            processingCommand_ = true;
            reportNotificationDeferred_ = false;
            result = onCommand(decodedCommand.command);
            hasLastCommand_ = true;
            lastCommandSequence_ = decodedCommand.sequence;
            lastCommand_ = decodedCommand.command;
            lastCommandResult_ = result;
            std::printf("COMMAND_DEDUP: duplicate=no sequence=%u action=executed\n",
                        static_cast<unsigned>(decodedCommand.sequence));
        }
        std::printf("COMMAND_ACK: behavior_result=%u\n",
                    static_cast<unsigned>(result));
        std::uint8_t ackPayload[IsspPayloadSize]{};
        std::size_t ackPayloadLength = 0;
        const IsspResult encodeResult = encodeCommandAck(
            config_.deviceId,
            decodedCommand.sequence,
            decodedCommand.command.endpointId,
            result,
            ackPayload,
            sizeof(ackPayload),
            ackPayloadLength);
        if (encodeResult != IsspResult::Ok)
        {
            std::printf("COMMAND_ACK: encode_result=%u reason=encode_failed\n",
                        static_cast<unsigned>(encodeResult));
            if (!duplicate)
            {
                finishCommandProcessing();
            }
            return;
        }
        std::printf("COMMAND_ACK: encode_result=%u ack_status=%u\n",
                    static_cast<unsigned>(encodeResult),
                    static_cast<unsigned>(ackPayload[kDiagnosticValueOffset]));

        const IsspResult sendResult = transport_.sendReply(
            ackPayload,
            ackPayloadLength,
            replyContext);
        std::printf("COMMAND_ACK: send_reply_result=%u\n",
                    static_cast<unsigned>(sendResult));
        if (!duplicate)
        {
            finishCommandProcessing();
        }
        (void)sendResult;
    }

    IsspCommandResult IsspDevice::onCommand(const IsspCommand &command)
    {
        std::printf("BEHAVIOR_MATCH: total=%u\n",
                    static_cast<unsigned>(behaviorCount_));
        for (std::size_t index = 0; index < behaviorCount_; ++index)
        {
            IDeviceBehavior *behavior = behaviors_[index];
            if (behavior == nullptr)
            {
                std::printf("BEHAVIOR_MATCH: index=%u accepts=no reason=null_behavior total=%u\n",
                            static_cast<unsigned>(index),
                            static_cast<unsigned>(behaviorCount_));
                continue;
            }

            const bool accepts = behavior->accepts(command);
            std::printf("BEHAVIOR_MATCH: index=%u accepts=%s total=%u\n",
                        static_cast<unsigned>(index),
                        accepts ? "yes" : "no",
                        static_cast<unsigned>(behaviorCount_));
            if (accepts)
            {
                return behavior->handle(command);
            }
        }

        if (commandHandler_ != nullptr)
        {
            return commandHandler_(command, commandContext_);
        }

        return IsspCommandResult::Unsupported;
    }

} // namespace issp
