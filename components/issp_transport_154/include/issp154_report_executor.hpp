#pragma once

#include <atomic>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "issp_device.hpp"
#include "issp154_transport.hpp"

namespace issp
{

class Issp154ReportExecutor
{
public:
    Issp154ReportExecutor(IsspDevice &device, Issp154Transport &transport);

    IsspResult start();
    IsspResult processOne();
    /// Terminal, idempotent stop for the current boot: deregisters the pending
    /// report notification, interrupts the retry wait, waits in a bounded way
    /// for the transport attempt already in flight and ends the task without
    /// destroying the executor. Returns Ok when the task has terminated and
    /// Busy when the bounded wait expired; a Busy result means the task may
    /// still be inside a transport operation, so the caller must not tear the
    /// transport down. It is a no-op returning Ok when start() never completed
    /// successfully, and the executor cannot be restarted in the same boot.
    IsspResult stop();

private:
    static constexpr std::size_t kTaskStackSizeBytes = 4096;
    static constexpr std::size_t kTaskStackDepth =
        kTaskStackSizeBytes / sizeof(StackType_t);
    // Bounded wait for the attempt already in flight. The worst case observed
    // for one confirmed send is around 465 ms, so this leaves scheduling margin
    // while keeping the terminal sequence delimited.
    static constexpr std::uint32_t kStopTimeoutMs = 600;
    static constexpr std::uint32_t kStopPollIntervalMs = 10;

    static void runTask(void *context);
    static void notifyPendingReport(void *context);
    void run();
    /// Interruptible replacement for the retry delay: returns true when the stop
    /// was requested while waiting.
    bool waitRetryDelayOrStop();

    IsspDevice &device_;
    Issp154Transport &transport_;
    StaticTask_t taskControl_;
    StackType_t taskStack_[kTaskStackDepth];
    TaskHandle_t taskHandle_;
    // Distinguishes the stop notification from a pending-report notification,
    // which share the same task notification.
    std::atomic<bool> stopRequested_;
    std::atomic<bool> taskExited_;
};

} // namespace issp
