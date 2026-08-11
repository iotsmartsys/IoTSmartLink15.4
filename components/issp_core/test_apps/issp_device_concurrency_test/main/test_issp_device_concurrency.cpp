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
    issp::IsspDevice device({.deviceId = 1}, transport);
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

TEST_CASE("pending reports retain insertion order", "[issp_device][pending]")
{
    FakeTransport transport;
    issp::IsspDevice device({.deviceId = 1}, transport);
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
    issp::IsspDevice device({.deviceId = 1}, transport);
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
    issp::IsspDevice device({.deviceId = 1}, transport);
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

extern "C" void app_main()
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
