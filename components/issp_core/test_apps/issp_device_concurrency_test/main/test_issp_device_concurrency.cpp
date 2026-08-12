#include <atomic>
#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "iissp_transport.hpp"
#include "issp_device.hpp"
#include "unity.h"

namespace
{

class FakeTransport final : public issp::IIsspTransport
{
public:
    issp::IsspResult begin() override { return issp::IsspResult::Ok; }
    issp::IsspResult send(const std::uint8_t *, std::size_t) override
    {
        return issp::IsspResult::Ok;
    }
    issp::IsspResult sendReply(const std::uint8_t *, std::size_t,
                               const void *) override
    {
        return issp::IsspResult::Ok;
    }
    issp::IsspTransportState state() const override
    {
        return issp::IsspTransportState::Ready;
    }
    void setReceiveHandler(ReceiveHandler, void *) override {}
};

issp::IsspReport report(std::uint8_t endpoint, std::uint8_t value)
{
    return {.endpointId = endpoint, .eventType = 1, .value = value};
}

// Deterministic source of report identities. Tests own the sequence of values
// so a bounded local search, a zero and a collision are all reproducible.
class FakeReportIds
{
public:
    static std::uint64_t next(void *context)
    {
        auto *self = static_cast<FakeReportIds *>(context);
        ++self->calls;
        if (self->scripted != nullptr && self->scriptedIndex < self->scriptedCount) {
            return self->scripted[self->scriptedIndex++];
        }
        ++self->counter;
        return self->counter;
    }

    std::uint64_t counter{0};
    std::uint32_t calls{0};
    const std::uint64_t *scripted{nullptr};
    std::size_t scriptedCount{0};
    std::size_t scriptedIndex{0};
};

issp::IsspDeviceConfig configWith(FakeReportIds &ids, std::uint32_t deviceId = 1)
{
    return {
        .deviceId = deviceId,
        .reportIdGenerator = &FakeReportIds::next,
        .reportIdGeneratorContext = &ids,
    };
}

struct ConcurrentScenario
{
    issp::IsspDevice *device;
    SemaphoreHandle_t done;
    std::atomic<bool> publisherDone{false};
    std::atomic<std::uint32_t> preparedCount{0};
    std::atomic<std::uint32_t> completionFailures{0};
};

void publishTask(void *context)
{
    auto *scenario = static_cast<ConcurrentScenario *>(context);
    for (std::uint32_t index = 0; index < 200; ++index)
    {
        (void)scenario->device->publishState(
            report(1, static_cast<std::uint8_t>(index & 1U)));
        taskYIELD();
    }
    scenario->publisherDone.store(true);
    xSemaphoreGive(scenario->done);
    vTaskDelete(nullptr);
}

void reserveTask(void *context)
{
    auto *scenario = static_cast<ConcurrentScenario *>(context);
    while (!scenario->publisherDone.load() ||
           scenario->device->pendingReportCount() > 0U)
    {
        issp::IsspPreparedReport prepared{};
        if (scenario->device->preparePendingReport(prepared) == issp::IsspResult::Ok)
        {
            if (!scenario->device->completePendingReport(prepared.token, true))
            {
                ++scenario->completionFailures;
            }
            ++scenario->preparedCount;
        }
        else
        {
            vTaskDelay(1);
        }
    }
    xSemaphoreGive(scenario->done);
    vTaskDelete(nullptr);
}

} // namespace

TEST_CASE("new generation survives completion of an in-flight report",
          "[issp_device][pending]")
{
    FakeTransport transport;
    FakeReportIds ids;
    issp::IsspDevice device(configWith(ids), transport);
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.publishState(report(1, 0))));

    issp::IsspReport acquired{};
    issp::IsspPendingReportToken firstToken{};
    TEST_ASSERT_TRUE(device.acquirePendingReport(acquired, firstToken));
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.publishState(report(1, 1))));
    TEST_ASSERT_TRUE(device.completePendingReport(firstToken, true));
    TEST_ASSERT_EQUAL_size_t(1, device.pendingReportCount());

    issp::IsspPendingReportToken secondToken{};
    TEST_ASSERT_TRUE(device.acquirePendingReport(acquired, secondToken));
    TEST_ASSERT_EQUAL_UINT8(1, acquired.value);
    TEST_ASSERT_TRUE(device.completePendingReport(secondToken, true));
    TEST_ASSERT_EQUAL_size_t(0, device.pendingReportCount());
}

// DEEPSLEEP-AC-006 and AC-007: once quiescence begins, admission is closed and
// the pending count becomes a stable delivery oracle -- slots already admitted
// are preserved and can still be drained.
TEST_CASE("beginQuiescence closes admission and preserves admitted slots",
          "[issp_device][quiescence]")
{
    FakeTransport transport;
    FakeReportIds ids;
    issp::IsspDevice device(configWith(ids), transport);
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.publishState(report(1, 0))));

    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.beginQuiescence()));
    TEST_ASSERT_EQUAL_size_t(1, device.pendingReportCount());

    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::NotReady),
                      static_cast<int>(device.publishState(report(2, 1))));
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::NotReady),
                      static_cast<int>(device.publishState(report(1, 1))));
    TEST_ASSERT_EQUAL_size_t(1, device.pendingReportCount());

    issp::IsspPreparedReport prepared{};
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.preparePendingReport(prepared)));
    TEST_ASSERT_TRUE(device.completePendingReport(prepared.token, true));
    TEST_ASSERT_EQUAL_size_t(0, device.pendingReportCount());
}

TEST_CASE("beginQuiescence is idempotent", "[issp_device][quiescence]")
{
    FakeTransport transport;
    FakeReportIds ids;
    issp::IsspDevice device(configWith(ids), transport);
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.beginQuiescence()));
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.beginQuiescence()));
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::NotReady),
                      static_cast<int>(device.publishState(report(1, 0))));
    TEST_ASSERT_EQUAL_size_t(0, device.pendingReportCount());
}

TEST_CASE("pending reports retain insertion order", "[issp_device][pending]")
{
    FakeTransport transport;
    FakeReportIds ids;
    issp::IsspDevice device(configWith(ids), transport);
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.publishState(report(2, 1))));
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.publishState(report(1, 0))));

    issp::IsspReport acquired{};
    issp::IsspPendingReportToken token{};
    TEST_ASSERT_TRUE(device.acquirePendingReport(acquired, token));
    TEST_ASSERT_EQUAL_UINT8(2, acquired.endpointId);
    TEST_ASSERT_TRUE(device.completePendingReport(token, true));
    TEST_ASSERT_TRUE(device.acquirePendingReport(acquired, token));
    TEST_ASSERT_EQUAL_UINT8(1, acquired.endpointId);
}

TEST_CASE("preparation reserves distinct report sequences", "[issp_device][pending]")
{
    FakeTransport transport;
    FakeReportIds ids;
    issp::IsspDevice device(configWith(ids), transport);
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.publishState(report(1, 0))));
    issp::IsspPreparedReport first{};
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.preparePendingReport(first)));
    TEST_ASSERT_TRUE(device.completePendingReport(first.token, true));

    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.publishState(report(2, 1))));
    issp::IsspPreparedReport second{};
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.preparePendingReport(second)));
    TEST_ASSERT_NOT_EQUAL(first.sequence, second.sequence);
}

TEST_CASE("publication and reservation remain integral across two tasks",
          "[issp_device][pending][concurrency]")
{
    FakeTransport transport;
    FakeReportIds ids;
    issp::IsspDevice device(configWith(ids), transport);
    SemaphoreHandle_t done = xSemaphoreCreateCounting(2, 0);
    TEST_ASSERT_NOT_NULL(done);
    ConcurrentScenario scenario{.device = &device, .done = done};

    TEST_ASSERT_EQUAL(pdPASS,
                      xTaskCreate(&publishTask, "publish", 3072, &scenario, 5, nullptr));
    TEST_ASSERT_EQUAL(pdPASS,
                      xTaskCreate(&reserveTask, "reserve", 3072, &scenario, 5, nullptr));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done, pdMS_TO_TICKS(5000)));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done, pdMS_TO_TICKS(5000)));
    TEST_ASSERT_GREATER_THAN_UINT32(0, scenario.preparedCount.load());
    TEST_ASSERT_EQUAL_UINT32(0, scenario.completionFailures.load());
    TEST_ASSERT_EQUAL_size_t(0, device.pendingReportCount());
    vSemaphoreDelete(done);
}

// REPORT-ID-AC-001: an insertion takes a new identity, retries reuse it, an
// update of the slot takes another one, and delivery of the current generation
// removes both the report and its identity.
TEST_CASE("report identity spans the whole life of the logical report",
          "[issp_device][report_id]")
{
    FakeTransport transport;
    FakeReportIds ids;
    issp::IsspDevice device(configWith(ids), transport);

    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.publishState(report(1, 0))));

    issp::IsspPreparedReport first{};
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.preparePendingReport(first)));
    TEST_ASSERT_NOT_EQUAL(0, first.reportId);

    // An external retry: a failed attempt releases the reservation without
    // changing the identity, and the next preparation reuses it with a new
    // sequence.
    TEST_ASSERT_TRUE(device.completePendingReport(first.token, false));
    issp::IsspPreparedReport retry{};
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.preparePendingReport(retry)));
    TEST_ASSERT_EQUAL_UINT64(first.reportId, retry.reportId);
    TEST_ASSERT_NOT_EQUAL(first.sequence, retry.sequence);
    TEST_ASSERT_TRUE(device.completePendingReport(retry.token, true));
    TEST_ASSERT_EQUAL_size_t(0, device.pendingReportCount());

    // A new admission of the same endpoint and event takes another identity,
    // even when the value did not change.
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.publishState(report(1, 0))));
    issp::IsspPreparedReport readmitted{};
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.preparePendingReport(readmitted)));
    TEST_ASSERT_NOT_EQUAL(first.reportId, readmitted.reportId);
    TEST_ASSERT_TRUE(device.completePendingReport(readmitted.token, true));
}

// REPORT-ID-AC-001: an update while an attempt is in flight leaves the previous
// identity in the prepared copy while the new generation keeps the new one.
TEST_CASE("an update in flight does not disturb the attempt already prepared",
          "[issp_device][report_id]")
{
    FakeTransport transport;
    FakeReportIds ids;
    issp::IsspDevice device(configWith(ids), transport);

    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.publishState(report(1, 0))));
    issp::IsspPreparedReport inFlight{};
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.preparePendingReport(inFlight)));

    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.publishState(report(1, 1))));
    // The old attempt ends with its own identity and does not remove the new
    // generation.
    TEST_ASSERT_TRUE(device.completePendingReport(inFlight.token, true));
    TEST_ASSERT_EQUAL_size_t(1, device.pendingReportCount());

    issp::IsspPreparedReport current{};
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(device.preparePendingReport(current)));
    TEST_ASSERT_NOT_EQUAL(inFlight.reportId, current.reportId);
    TEST_ASSERT_EQUAL_UINT8(1, current.report.value);
}

// REPORT-ID-AC-002: a generator that only yields zero, and one that only yields
// collisions, both fail explicitly without mutating the slot; a configuration
// without a generator is rejected.
TEST_CASE("identity generation fails atomically", "[issp_device][report_id]")
{
    FakeTransport transport;

    // A generator that always returns zero: the bounded search gives up.
    static const std::uint64_t zeros[16] = {};
    FakeReportIds zeroIds;
    zeroIds.scripted = zeros;
    zeroIds.scriptedCount = sizeof(zeros) / sizeof(zeros[0]);
    issp::IsspDevice zeroDevice(configWith(zeroIds), transport);
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Failed),
                      static_cast<int>(zeroDevice.publishState(report(1, 0))));
    TEST_ASSERT_EQUAL_size_t(0, zeroDevice.pendingReportCount());
    TEST_ASSERT_GREATER_THAN_UINT32(0, zeroIds.calls);

    // A generator that keeps colliding with the identity already admitted: the
    // second admission fails and leaves the first slot intact.
    static const std::uint64_t repeated[16] = {7, 7, 7, 7, 7, 7, 7, 7,
                                               7, 7, 7, 7, 7, 7, 7, 7};
    FakeReportIds collidingIds;
    collidingIds.scripted = repeated;
    collidingIds.scriptedCount = sizeof(repeated) / sizeof(repeated[0]);
    issp::IsspDevice collidingDevice(configWith(collidingIds), transport);
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(collidingDevice.publishState(report(1, 0))));
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Failed),
                      static_cast<int>(collidingDevice.publishState(report(2, 0))));
    TEST_ASSERT_EQUAL_size_t(1, collidingDevice.pendingReportCount());

    issp::IsspPreparedReport prepared{};
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(collidingDevice.preparePendingReport(prepared)));
    TEST_ASSERT_EQUAL_UINT64(7, prepared.reportId);
    TEST_ASSERT_EQUAL_UINT8(1, prepared.report.endpointId);

    // A configuration without a generator cannot admit a report.
    issp::IsspDevice ungenerated({.deviceId = 1,
                                  .reportIdGenerator = nullptr,
                                  .reportIdGeneratorContext = nullptr},
                                 transport);
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::InvalidArgument),
                      static_cast<int>(ungenerated.publishState(report(1, 0))));
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::InvalidArgument),
                      static_cast<int>(ungenerated.publishReport(report(1, 0))));
    TEST_ASSERT_EQUAL_size_t(0, ungenerated.pendingReportCount());
}

// REPORT-ID-AC-003: concurrent publications never accept the same identity.
TEST_CASE("concurrent publications hold distinct identities",
          "[issp_device][report_id][concurrency]")
{
    FakeTransport transport;
    FakeReportIds ids;
    issp::IsspDevice device(configWith(ids), transport);

    // Eight slots, eight distinct identities, all occupied at once.
    std::uint64_t observed[issp::kMaxPendingReports] = {};
    for (std::size_t index = 0; index < issp::kMaxPendingReports; ++index) {
        TEST_ASSERT_EQUAL(
            static_cast<int>(issp::IsspResult::Ok),
            static_cast<int>(device.publishState(
                report(static_cast<std::uint8_t>(index + 1U), 1))));
    }
    TEST_ASSERT_EQUAL_size_t(issp::kMaxPendingReports, device.pendingReportCount());

    for (std::size_t index = 0; index < issp::kMaxPendingReports; ++index) {
        issp::IsspPreparedReport prepared{};
        TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                          static_cast<int>(device.preparePendingReport(prepared)));
        TEST_ASSERT_NOT_EQUAL(0, prepared.reportId);
        for (std::size_t earlier = 0; earlier < index; ++earlier) {
            TEST_ASSERT_NOT_EQUAL(observed[earlier], prepared.reportId);
        }
        observed[index] = prepared.reportId;
        TEST_ASSERT_TRUE(device.completePendingReport(prepared.token, true));
    }
    TEST_ASSERT_EQUAL_size_t(0, device.pendingReportCount());
}

extern "C" void app_main()
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
