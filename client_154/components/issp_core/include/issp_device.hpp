#pragma once

#include <array>
#include <cstddef>

#include "ibehavior_state_publisher.hpp"
#include "iissp_transport.hpp"
#include "issp_limits.hpp"
#include "issp_protocol.hpp"
#include "issp_types.hpp"

namespace issp
{

class IDeviceBehavior;

struct IsspPreparedReport
{
    IsspPendingReportToken token;
    std::uint32_t deviceId;
    std::uint16_t sequence;
    IsspReport report;
    std::array<std::uint8_t, IsspPayloadSize> payload;
    std::size_t payloadLength;
};

class IsspDevice : public IBehaviorStatePublisher
{
public:
    using CommandHandler = IsspCommandResult (*)(const IsspCommand &command, void *context);
    using PendingReportHandler = void (*)(void *context);

    IsspDevice(const IsspDeviceConfig &config, IIsspTransport &transport);

    IsspResult addBehavior(IDeviceBehavior &behavior);
    /// Initializes the registered behaviors and activates receive processing.
    /// The application must start the transport before calling this method.
    IsspResult start();
    std::uint32_t deviceId() const;
    IsspTransportState transportState() const;
    IsspResult publishState(const IsspReport &report) override;
    IsspResult publishReport(const IsspReport &report);
    /// Pending-report publication, reservation, and completion are serial;
    /// concurrent callers are not supported.
    std::size_t pendingReportCount() const;
    /// Copy the oldest available pending report without reserving it.
    /// Reports already in flight are ignored.
    bool peekPendingReport(IsspReport &report) const;
    bool acquirePendingReport(IsspReport &report, IsspPendingReportToken &token);
    bool completePendingReport(const IsspPendingReportToken &token, bool delivered);
    IsspResult preparePendingReport(IsspPreparedReport &preparedReport);
    /// The handler is called after a pending report is inserted or updated.
    /// Publications made while processing a command are notified after the
    /// command reply attempt completes.
    /// It must be non-blocking and must not process or transmit the report.
    void setPendingReportHandler(PendingReportHandler handler, void *context);

    // Called in the same execution context used by the transport receive handler.
    // The handler must be non-blocking, allocation-free, and safe for that context.
    void setCommandHandler(CommandHandler handler, void *context);

private:
    struct PendingReportSlot
    {
        IsspReport report;
        std::uint32_t generation;
        std::uint32_t inFlightGeneration;
        std::uint32_t insertionOrder;
        bool occupied;
        bool inFlight;
    };

    static void advanceGeneration(std::uint32_t &generation);
    void notifyPendingReport();
    void finishCommandProcessing();
    std::uint32_t nextPendingReportOrder();
    void normalizePendingReportOrders();

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
    PendingReportHandler pendingReportHandler_;
    void *pendingReportContext_;
    bool processingCommand_;
    bool reportNotificationDeferred_;
    std::uint16_t reportSequence_;
    std::array<PendingReportSlot, kMaxPendingReports> pendingReports_;
    std::size_t pendingReportCount_;
    std::uint32_t nextPendingReportOrder_;
};

} // namespace issp
