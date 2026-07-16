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
          reportSequence_(0)
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
        return transport_.begin();
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
        return publishReport(report);
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
        if (decodeCommand(data, length, config_.deviceId, decodedCommand) != IsspResult::Ok)
        {
            return;
        }

        const IsspCommandResult result = onCommand(decodedCommand.command);
        std::uint8_t ackPayload[IsspPayloadSize]{};
        std::size_t ackPayloadLength = 0;
        if (encodeCommandAck(
                config_.deviceId,
                decodedCommand.sequence,
                decodedCommand.command.endpointId,
                result,
                ackPayload,
                sizeof(ackPayload),
                ackPayloadLength) != IsspResult::Ok)
        {
            return;
        }

        const IsspResult sendResult = transport_.sendReply(
            ackPayload,
            ackPayloadLength,
            replyContext);
        (void)sendResult;
    }

    IsspCommandResult IsspDevice::onCommand(const IsspCommand &command)
    {
        for (std::size_t index = 0; index < behaviorCount_; ++index)
        {
            IDeviceBehavior *behavior = behaviors_[index];
            if (behavior == nullptr)
            {
                continue;
            }

            if (behavior->accepts(command))
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
