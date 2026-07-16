#include "issp_device.hpp"
#include "issp_protocol.hpp"

namespace issp
{

    IsspDevice::IsspDevice(const IsspDeviceConfig &config, IIsspTransport &transport)
        : config_(config),
          transport_(transport),
          commandHandler_(nullptr),
          commandContext_(nullptr)
    {
    }

    IsspResult IsspDevice::start()
    {
        transport_.setReceiveHandler(&IsspDevice::handleReceive, this);
        return transport_.begin();
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

        (void)replyContext;
        (void)ackPayload;
        (void)ackPayloadLength;
    }

    IsspCommandResult IsspDevice::onCommand(const IsspCommand &command)
    {
        if (commandHandler_ == nullptr)
        {
            return IsspCommandResult::Unsupported;
        }

        return commandHandler_(command, commandContext_);
    }

} // namespace issp
