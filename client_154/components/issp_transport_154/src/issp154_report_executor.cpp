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
    ESP_LOGI("REPORT_EXECUTOR", "prepare_pending_report begin");
    IsspPreparedReport prepared{};
    const IsspResult prepareResult = device_.preparePendingReport(prepared);
    ESP_LOGI("REPORT_EXECUTOR",
             "prepare_pending_report result=%u",
             static_cast<unsigned>(prepareResult));
    if (prepareResult != IsspResult::Ok) {
        return prepareResult;
    }

    ESP_LOGI("REPORT_EXECUTOR",
             "prepared slot=%u generation=%lu endpoint=%u event=%u value=%u sequence=%u",
             static_cast<unsigned>(prepared.token.slotIndex),
             static_cast<unsigned long>(prepared.token.generation),
             static_cast<unsigned>(prepared.report.endpointId),
             static_cast<unsigned>(prepared.report.eventType),
             static_cast<unsigned>(prepared.report.value),
             static_cast<unsigned>(prepared.sequence));

    const Issp154AckExpectation expectation{
        .deviceId = prepared.deviceId,
        .sequence = prepared.sequence,
        .endpointId = prepared.report.endpointId,
    };
    Issp154ConfirmedSendSummary summary{};
    ESP_LOGI("REPORT_EXECUTOR",
             "confirmed_send begin slot=%u generation=%lu",
             static_cast<unsigned>(prepared.token.slotIndex),
             static_cast<unsigned long>(prepared.token.generation));
    const IsspResult sendResult = transport_.sendConfirmed(
        prepared.payload.data(),
        prepared.payloadLength,
        expectation,
        kReportAckTimeoutMs,
        summary);
    ESP_LOGI("REPORT_EXECUTOR",
             "confirmed_send result=%u attempts=%u attempt_result=%u ack_status=%u",
             static_cast<unsigned>(sendResult),
             static_cast<unsigned>(summary.attempts),
             static_cast<unsigned>(summary.attemptResult),
             static_cast<unsigned>(summary.ackStatus));
    const bool delivered =
        sendResult == IsspResult::Ok &&
        summary.attemptResult == Issp154AckAttemptResult::AckReceived &&
        summary.ackStatus == IsspAckStatus::Ok;

    const bool completeResult =
        device_.completePendingReport(prepared.token, delivered);
    ESP_LOGI("REPORT_EXECUTOR",
             "complete_pending_report result=%s delivered=%s slot=%u generation=%lu",
             completeResult ? "ok" : "failed",
             delivered ? "yes" : "no",
             static_cast<unsigned>(prepared.token.slotIndex),
             static_cast<unsigned long>(prepared.token.generation));
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
    ESP_LOGI("REPORT_EXECUTOR",
             "pending_report_notification received context=%s",
             context != nullptr ? "valid" : "null");
    if (context == nullptr) {
        return;
    }

    auto *executor = static_cast<Issp154ReportExecutor *>(context);
    if (executor->taskHandle_ != nullptr) {
        ESP_LOGI("REPORT_EXECUTOR", "pending_report_notification task_notified=yes");
        xTaskNotifyGive(executor->taskHandle_);
    } else {
        ESP_LOGI("REPORT_EXECUTOR", "pending_report_notification task_notified=no");
    }
}

void Issp154ReportExecutor::run()
{
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI("REPORT_EXECUTOR", "task awakened");

        for (;;) {
            IsspReport report{};
            ESP_LOGI("REPORT_EXECUTOR", "checking_pending_report");
            if (!device_.peekPendingReport(report)) {
                ESP_LOGI("REPORT_EXECUTOR", "checking_pending_report result=none");
                break;
            }
            ESP_LOGI("REPORT_EXECUTOR",
                     "checking_pending_report result=available endpoint=%u event=%u value=%u",
                     static_cast<unsigned>(report.endpointId),
                     static_cast<unsigned>(report.eventType),
                     static_cast<unsigned>(report.value));
            const IsspResult processResult = processOne();
            ESP_LOGI("REPORT_EXECUTOR",
                     "process_one result=%u",
                     static_cast<unsigned>(processResult));
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
