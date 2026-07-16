#pragma once

#include <array>
#include <cstddef>

#include "ibehavior_state_publisher.hpp"
#include "iissp_transport.hpp"
#include "issp_limits.hpp"
#include "issp_types.hpp"

namespace issp
{

class IDeviceBehavior;

class IsspDevice : public IBehaviorStatePublisher
{
public:
    using CommandHandler = IsspCommandResult (*)(const IsspCommand &command, void *context);

    IsspDevice(const IsspDeviceConfig &config, IIsspTransport &transport);

    IsspResult addBehavior(IDeviceBehavior &behavior);
    IsspResult start();
    std::uint32_t deviceId() const;
    IsspTransportState transportState() const;
    IsspResult publishState(const IsspReport &report) override;
    IsspResult publishReport(const IsspReport &report);

    // Called in the same execution context used by the transport receive handler.
    // The handler must be non-blocking, allocation-free, and safe for that context.
    void setCommandHandler(CommandHandler handler, void *context);

private:
    static void handleReceive(const std::uint8_t *data,
                              std::size_t length,
                              const void *replyContext,
                              void *context);
    void onReceive(const std::uint8_t *data,
                   std::size_t length,
                   const void *replyContext);
    IsspCommandResult onCommand(const IsspCommand &command);

    IsspDeviceConfig config_;
    IIsspTransport &transport_;
    std::array<IDeviceBehavior *, kMaxDeviceBehaviors> behaviors_;
    std::size_t behaviorCount_;
    CommandHandler commandHandler_;
    void *commandContext_;
    std::uint16_t reportSequence_;
};

} // namespace issp
