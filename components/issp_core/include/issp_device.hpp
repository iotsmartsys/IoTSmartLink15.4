#pragma once

#include <array>
#include <cstddef>

#include "freertos/FreeRTOS.h"
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
    /// Logical identity of the report, stable across every attempt.
    std::uint64_t reportId;
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
    /// Terminal, idempotent quiescence for the current boot. Atomically closes
    /// the dispatch of new commands and the admission of new reports: commands
    /// still received are answered with IsspCommandResult::Failed, new
    /// publications return IsspResult::NotReady, and every slot already admitted
    /// is preserved so the pending count remains a stable delivery oracle. It
    /// does not stop the transport and cannot be reverted in the same boot.
    IsspResult beginQuiescence();
    /// Pending-report publication, reservation, completion, and inspection are
    /// internally serialized. Callbacks, encoding, notifications, and transport
    /// operations execute outside the critical section.
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
        // Identity of the admission currently held by the slot. A concurrent
        // update overwrites it, while the attempt already in flight keeps the
        // previous identity in its prepared copy and ACK expectation.
        std::uint64_t reportId;
        std::uint32_t generation;
        std::uint32_t inFlightGeneration;
        std::uint32_t insertionOrder;
        bool occupied;
        bool inFlight;
    };

    // Bounded local search: a generator that only yields zero or collisions
    // fails explicitly instead of looping or mutating a slot partially.
    static constexpr std::size_t kReportIdGenerationAttempts = 8;

    static void advanceGeneration(std::uint32_t &generation);
    bool reportIdInUseLocked(std::uint64_t reportId) const;
    bool acquirePendingReportLocked(IsspReport &report,
                                    IsspPendingReportToken &token,
                                    std::uint64_t &reportId);
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
    mutable portMUX_TYPE reportLock_ = portMUX_INITIALIZER_UNLOCKED;
    bool processingCommand_;
    bool reportNotificationDeferred_;
    bool quiescing_;
    bool hasLastCommand_;
    std::uint16_t lastCommandSequence_;
    IsspCommand lastCommand_;
    IsspCommandResult lastCommandResult_;
    std::uint16_t reportSequence_;
    std::array<PendingReportSlot, kMaxPendingReports> pendingReports_;
    std::size_t pendingReportCount_;
    std::uint32_t nextPendingReportOrder_;
};

} // namespace issp
