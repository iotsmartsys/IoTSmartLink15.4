#include "issp_device.hpp"
#include "idevice_behavior.hpp"
#include "issp_protocol.hpp"

namespace issp
{

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
          quiescing_(false),
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

    IsspResult IsspDevice::beginQuiescence()
    {
        portENTER_CRITICAL(&reportLock_);
        quiescing_ = true;
        portEXIT_CRITICAL(&reportLock_);
        return IsspResult::Ok;
    }

    IsspResult IsspDevice::publishState(const IsspReport &report)
    {
        bool shouldNotify = false;
        portENTER_CRITICAL(&reportLock_);
        if (quiescing_)
        {
            portEXIT_CRITICAL(&reportLock_);
            return IsspResult::NotReady;
        }
        for (std::size_t index = 0; index < pendingReports_.size(); ++index)
        {
            PendingReportSlot &slot = pendingReports_[index];
            if (slot.occupied &&
                slot.report.endpointId == report.endpointId &&
                slot.report.eventType == report.eventType)
            {
                slot.report = report;
                advanceGeneration(slot.generation);
                if (processingCommand_)
                {
                    reportNotificationDeferred_ = true;
                }
                else
                {
                    shouldNotify = true;
                }
                portEXIT_CRITICAL(&reportLock_);
                if (shouldNotify)
                {
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
                if (processingCommand_)
                {
                    reportNotificationDeferred_ = true;
                }
                else
                {
                    shouldNotify = true;
                }
                portEXIT_CRITICAL(&reportLock_);
                if (shouldNotify)
                {
                    notifyPendingReport();
                }
                return IsspResult::Ok;
            }
        }

        portEXIT_CRITICAL(&reportLock_);
        return IsspResult::Failed;
    }

    IsspResult IsspDevice::publishReport(const IsspReport &report)
    {
        portENTER_CRITICAL(&reportLock_);
        if (quiescing_)
        {
            portEXIT_CRITICAL(&reportLock_);
            return IsspResult::NotReady;
        }
        const std::uint16_t sequence = reportSequence_;
        ++reportSequence_;
        portEXIT_CRITICAL(&reportLock_);

        std::uint8_t payload[IsspPayloadSize]{};
        std::size_t payloadLength = 0;
        const IsspResult encodeResult = encodeReport(
            config_.deviceId,
            sequence,
            report,
            payload,
            sizeof(payload),
            payloadLength);
        if (encodeResult != IsspResult::Ok)
        {
            return encodeResult;
        }

        return transport_.send(payload, payloadLength);
    }

    std::size_t IsspDevice::pendingReportCount() const
    {
        portENTER_CRITICAL(&reportLock_);
        const std::size_t count = pendingReportCount_;
        portEXIT_CRITICAL(&reportLock_);
        return count;
    }

    bool IsspDevice::peekPendingReport(IsspReport &report) const
    {
        portENTER_CRITICAL(&reportLock_);
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
            portEXIT_CRITICAL(&reportLock_);
            return false;
        }

        report = oldestSlot->report;
        portEXIT_CRITICAL(&reportLock_);
        return true;
    }

    bool IsspDevice::acquirePendingReportLocked(IsspReport &report,
                                                IsspPendingReportToken &token)
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

    bool IsspDevice::acquirePendingReport(IsspReport &report,
                                          IsspPendingReportToken &token)
    {
        portENTER_CRITICAL(&reportLock_);
        const bool acquired = acquirePendingReportLocked(report, token);
        portEXIT_CRITICAL(&reportLock_);
        return acquired;
    }

    bool IsspDevice::completePendingReport(const IsspPendingReportToken &token, bool delivered)
    {
        portENTER_CRITICAL(&reportLock_);
        if (token.slotIndex >= kMaxPendingReports)
        {
            portEXIT_CRITICAL(&reportLock_);
            return false;
        }

        PendingReportSlot &slot = pendingReports_[token.slotIndex];
        if (!slot.occupied || !slot.inFlight ||
            slot.inFlightGeneration != token.generation)
        {
            portEXIT_CRITICAL(&reportLock_);
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

        portEXIT_CRITICAL(&reportLock_);
        return true;
    }

    IsspResult IsspDevice::preparePendingReport(IsspPreparedReport &preparedReport)
    {
        preparedReport = {};

        IsspReport report{};
        IsspPendingReportToken token{};
        portENTER_CRITICAL(&reportLock_);
        if (!acquirePendingReportLocked(report, token))
        {
            portEXIT_CRITICAL(&reportLock_);
            return IsspResult::NotReady;
        }
        const std::uint16_t sequence = reportSequence_;
        ++reportSequence_;
        portEXIT_CRITICAL(&reportLock_);

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
        portENTER_CRITICAL(&reportLock_);
        pendingReportHandler_ = handler;
        pendingReportContext_ = context;
        portEXIT_CRITICAL(&reportLock_);
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
        portENTER_CRITICAL(&reportLock_);
        PendingReportHandler handler = pendingReportHandler_;
        void *context = pendingReportContext_;
        portEXIT_CRITICAL(&reportLock_);
        if (handler != nullptr)
        {
            handler(context);
        }
    }

    void IsspDevice::finishCommandProcessing()
    {
        portENTER_CRITICAL(&reportLock_);
        const bool notifyDeferredReport = reportNotificationDeferred_;
        processingCommand_ = false;
        reportNotificationDeferred_ = false;
        portEXIT_CRITICAL(&reportLock_);
        if (notifyDeferredReport)
        {
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
        IsspDecodedCommand decodedCommand{};
        const IsspResult decodeResult =
            decodeCommand(data, length, config_.deviceId, decodedCommand);
        if (decodeResult != IsspResult::Ok)
        {
            return;
        }

        const bool duplicate = hasLastCommand_ &&
            lastCommandSequence_ == decodedCommand.sequence &&
            lastCommand_.endpointId == decodedCommand.command.endpointId &&
            lastCommand_.eventType == decodedCommand.command.eventType &&
            lastCommand_.value == decodedCommand.command.value;

        IsspCommandResult result = lastCommandResult_;
        if (!duplicate)
        {
            portENTER_CRITICAL(&reportLock_);
            processingCommand_ = true;
            reportNotificationDeferred_ = false;
            portEXIT_CRITICAL(&reportLock_);
            result = onCommand(decodedCommand.command);
            hasLastCommand_ = true;
            lastCommandSequence_ = decodedCommand.sequence;
            lastCommand_ = decodedCommand.command;
            lastCommandResult_ = result;
        }
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
            if (!duplicate)
            {
                finishCommandProcessing();
            }
            return;
        }
        const IsspResult sendResult = transport_.sendReply(
            ackPayload,
            ackPayloadLength,
            replyContext);
        if (!duplicate)
        {
            finishCommandProcessing();
        }
        (void)sendResult;
    }

    IsspCommandResult IsspDevice::onCommand(const IsspCommand &command)
    {
        portENTER_CRITICAL(&reportLock_);
        const bool quiescing = quiescing_;
        portEXIT_CRITICAL(&reportLock_);
        if (quiescing)
        {
            // Dispatch is closed for this boot: the command is still acknowledged,
            // with Failed, instead of reaching a behavior that already quiesced.
            return IsspCommandResult::Failed;
        }

        for (std::size_t index = 0; index < behaviorCount_; ++index)
        {
            IDeviceBehavior *behavior = behaviors_[index];
            if (behavior == nullptr)
            {
                continue;
            }

            const bool accepts = behavior->accepts(command);
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
