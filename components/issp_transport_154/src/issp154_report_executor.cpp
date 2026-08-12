#include "issp154_report_executor.hpp"

#include "esp_log.h"
#include "esp_timer.h"

namespace issp
{

namespace
{

constexpr std::uint32_t kReportAckTimeoutMs = 50;
constexpr std::uint32_t kPendingReportRetryDelayMs = 1000;
constexpr UBaseType_t kReportTaskPriority = tskIDLE_PRIORITY + 3;

bool isRetryableResult(IsspResult result)
{
    return result == IsspResult::NotReady ||
           result == IsspResult::Busy ||
           result == IsspResult::Failed;
}

} // namespace

Issp154ReportExecutor::Issp154ReportExecutor(
    IsspDevice &device,
    Issp154Transport &transport)
    : device_(device),
      transport_(transport),
      taskControl_{},
      taskStack_{},
      taskHandle_(nullptr),
      stopRequested_(false),
      taskExited_(false)
{
}

IsspResult Issp154ReportExecutor::start()
{
    if (transport_.state() != IsspTransportState::Ready) {
        return IsspResult::NotReady;
    }
    if (taskHandle_ != nullptr) {
        return IsspResult::Busy;
    }

    TaskHandle_t taskHandle = xTaskCreateStatic(
        &Issp154ReportExecutor::runTask,
        "issp154_report_tx",
        kTaskStackDepth,
        this,
        kReportTaskPriority,
        taskStack_,
        &taskControl_);
    if (taskHandle == nullptr) {
        return IsspResult::Failed;
    }

    taskHandle_ = taskHandle;
    device_.setPendingReportHandler(
        &Issp154ReportExecutor::notifyPendingReport, this);

    IsspReport report{};
    if (device_.peekPendingReport(report)) {
        xTaskNotifyGive(taskHandle_);
    }
    return IsspResult::Ok;
}

IsspResult Issp154ReportExecutor::stop()
{
    // taskHandle_ is assigned only after xTaskCreateStatic() returned a valid
    // handle, so a null handle identifies exactly the executor never started.
    if (taskHandle_ == nullptr) {
        return IsspResult::Ok;
    }
    if (taskExited_.load(std::memory_order_acquire)) {
        return IsspResult::Ok;
    }

    stopRequested_.store(true, std::memory_order_release);
    device_.setPendingReportHandler(nullptr, nullptr);
    xTaskNotifyGive(taskHandle_);

    const std::int64_t deadlineUs =
        esp_timer_get_time() + static_cast<std::int64_t>(kStopTimeoutMs) * 1000;
    while (!taskExited_.load(std::memory_order_acquire)) {
        if (esp_timer_get_time() >= deadlineUs) {
            ESP_LOGW("REPORT_EXECUTOR",
                     "stop timeout_ms=%lu state=attempt_in_flight",
                     static_cast<unsigned long>(kStopTimeoutMs));
            return IsspResult::Busy;
        }
        vTaskDelay(pdMS_TO_TICKS(kStopPollIntervalMs));
    }

    ESP_LOGI("REPORT_EXECUTOR", "stop completed");
    return IsspResult::Ok;
}

IsspResult Issp154ReportExecutor::processOne()
{
    IsspPreparedReport prepared{};
    const IsspResult prepareResult = device_.preparePendingReport(prepared);
    if (prepareResult != IsspResult::Ok) {
        return prepareResult;
    }

    const Issp154AckExpectation expectation{
        .deviceId = prepared.deviceId,
        .sequence = prepared.sequence,
        .reportId = prepared.reportId,
        .endpointId = prepared.report.endpointId,
    };
    Issp154ConfirmedSendSummary summary{};
    const IsspResult sendResult = transport_.sendConfirmed(
        prepared.payload.data(),
        prepared.payloadLength,
        expectation,
        kReportAckTimeoutMs,
        summary);
    const bool delivered =
        sendResult == IsspResult::Ok &&
        summary.attemptResult == Issp154AckAttemptResult::AckReceived &&
        summary.ackStatus == IsspAckStatus::Ok;

    const bool completeResult =
        device_.completePendingReport(prepared.token, delivered);
    if (!completeResult) {
        return IsspResult::Failed;
    }
    if (!delivered) {
        if (sendResult == IsspResult::InvalidArgument ||
            sendResult == IsspResult::NotReady ||
            sendResult == IsspResult::Busy) {
            return sendResult;
        }
        return IsspResult::Failed;
    }

    return IsspResult::Ok;
}

void Issp154ReportExecutor::runTask(void *context)
{
    if (context != nullptr) {
        static_cast<Issp154ReportExecutor *>(context)->run();
    }
    vTaskDelete(nullptr);
}

void Issp154ReportExecutor::notifyPendingReport(void *context)
{
    if (context == nullptr) {
        return;
    }

    auto *executor = static_cast<Issp154ReportExecutor *>(context);
    if (executor->taskHandle_ != nullptr) {
        xTaskNotifyGive(executor->taskHandle_);
    }
}

bool Issp154ReportExecutor::waitRetryDelayOrStop()
{
    // Preserves the retry duration in force while allowing stop() to interrupt
    // it, without xTaskAbortDelay, a Kconfig option or an external dependency.
    (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kPendingReportRetryDelayMs));
    return stopRequested_.load(std::memory_order_acquire);
}

void Issp154ReportExecutor::run()
{
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (stopRequested_.load(std::memory_order_acquire)) {
            break;
        }
        for (;;) {
            IsspReport report{};
            if (!device_.peekPendingReport(report)) {
                break;
            }
            const IsspResult processResult = processOne();
            if (stopRequested_.load(std::memory_order_acquire)) {
                break;
            }
            if (processResult != IsspResult::Ok) {
                if (isRetryableResult(processResult)) {
                    ESP_LOGW("REPORT_EXECUTOR",
                             "pending_report_retry scheduled_in_ms=%lu result=%u",
                             static_cast<unsigned long>(kPendingReportRetryDelayMs),
                             static_cast<unsigned>(processResult));
                    if (waitRetryDelayOrStop()) {
                        break;
                    }
                    continue;
                }
                ESP_LOGE("REPORT_EXECUTOR",
                         "pending_report_retry disabled reason=non_retryable result=%u",
                         static_cast<unsigned>(processResult));
                break;
            }
        }
        if (stopRequested_.load(std::memory_order_acquire)) {
            break;
        }
    }

    taskExited_.store(true, std::memory_order_release);
}

} // namespace issp
