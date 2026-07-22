#include "issp154_report_executor.hpp"

#include "esp_log.h"

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
      taskHandle_(nullptr)
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
    if (context == nullptr) {
        return;
    }
    static_cast<Issp154ReportExecutor *>(context)->run();
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

void Issp154ReportExecutor::run()
{
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        for (;;) {
            IsspReport report{};
            if (!device_.peekPendingReport(report)) {
                break;
            }
            const IsspResult processResult = processOne();
            if (processResult != IsspResult::Ok) {
                if (isRetryableResult(processResult)) {
                    ESP_LOGW("REPORT_EXECUTOR",
                             "pending_report_retry scheduled_in_ms=%lu result=%u",
                             static_cast<unsigned long>(kPendingReportRetryDelayMs),
                             static_cast<unsigned>(processResult));
                    vTaskDelay(pdMS_TO_TICKS(kPendingReportRetryDelayMs));
                    continue;
                }
                ESP_LOGE("REPORT_EXECUTOR",
                         "pending_report_retry disabled reason=non_retryable result=%u",
                         static_cast<unsigned>(processResult));
                break;
            }
        }
    }
}

} // namespace issp
