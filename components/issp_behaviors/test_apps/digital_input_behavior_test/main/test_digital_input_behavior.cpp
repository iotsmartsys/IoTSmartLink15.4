#include <array>
#include <cstddef>

#include "digital_input_behavior.hpp"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ibehavior_state_publisher.hpp"
#include "unity.h"

namespace
{

class FakePublisher final : public issp::IBehaviorStatePublisher
{
public:
    issp::IsspResult publishState(const issp::IsspReport &report) override
    {
        if (result != issp::IsspResult::Ok)
        {
            return result;
        }
        if (count < reports.size())
        {
            reports[count] = report;
            timestampsUs[count] = esp_timer_get_time();
        }
        ++count;
        return issp::IsspResult::Ok;
    }

    issp::IsspResult result = issp::IsspResult::Ok;
    std::array<issp::IsspReport, 8> reports{};
    std::array<std::int64_t, 8> timestampsUs{};
    std::size_t count = 0;
};

int readLevel(void *context)
{
    return *static_cast<int *>(context);
}

issp::DigitalInputConfig makeConfig()
{
    return {
        .endpointId = 1,
        .eventType = 1,
        .pin = GPIO_NUM_14,
        .activeLevel = 1,
        .pull = issp::DigitalInputPull::PullUp,
        .reportOnStart = true,
        .samplePeriodMs = 10,
        .samplesPerWindow = 5,
        .majorityThreshold = 3,
        .consecutiveWindows = 2,
    };
}

void feed(issp::DigitalInputBehavior &behavior,
          const std::array<std::uint32_t, 5> &levels)
{
    for (const std::uint32_t level : levels)
    {
        TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                          static_cast<int>(behavior.sampleForTest(level)));
    }
}

constexpr std::array<std::uint32_t, 5> kHighWindow{1, 1, 0, 1, 0};
constexpr std::array<std::uint32_t, 5> kLowWindow{0, 0, 1, 0, 1};

} // namespace

TEST_CASE("two high-majority windows publish initial open state", "[digital_input]")
{
    int level = 1;
    FakePublisher publisher;
    issp::DigitalInputBehavior behavior(makeConfig(), &readLevel, &level);
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(behavior.beginForTest(publisher)));
    feed(behavior, kHighWindow);
    feed(behavior, kHighWindow);
    TEST_ASSERT_TRUE(behavior.hasConfirmedState());
    TEST_ASSERT_TRUE(behavior.state());
    TEST_ASSERT_EQUAL_size_t(1, publisher.count);
    TEST_ASSERT_EQUAL_UINT8(1, publisher.reports[0].value);
}

TEST_CASE("two low-majority windows publish initial closed state", "[digital_input]")
{
    int level = 0;
    FakePublisher publisher;
    issp::DigitalInputBehavior behavior(makeConfig(), &readLevel, &level);
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(behavior.beginForTest(publisher)));
    feed(behavior, kLowWindow);
    feed(behavior, kLowWindow);
    TEST_ASSERT_FALSE(behavior.state());
    TEST_ASSERT_EQUAL_UINT8(0, publisher.reports[0].value);
}

TEST_CASE("a stable transition publishes once and duplicates are suppressed",
          "[digital_input]")
{
    int level = 0;
    FakePublisher publisher;
    issp::DigitalInputBehavior behavior(makeConfig(), &readLevel, &level);
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(behavior.beginForTest(publisher)));
    feed(behavior, kLowWindow);
    feed(behavior, kLowWindow);
    feed(behavior, kHighWindow);
    feed(behavior, kHighWindow);
    feed(behavior, kHighWindow);
    feed(behavior, kHighWindow);
    TEST_ASSERT_EQUAL_size_t(2, publisher.count);
    TEST_ASSERT_EQUAL_UINT8(1, publisher.reports[1].value);
}

TEST_CASE("nonconsecutive new classifications do not publish", "[digital_input]")
{
    int level = 0;
    FakePublisher publisher;
    issp::DigitalInputBehavior behavior(makeConfig(), &readLevel, &level);
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(behavior.beginForTest(publisher)));
    feed(behavior, kLowWindow);
    feed(behavior, kLowWindow);
    feed(behavior, kHighWindow);
    feed(behavior, kLowWindow);
    TEST_ASSERT_EQUAL_size_t(1, publisher.count);
    TEST_ASSERT_FALSE(behavior.state());
}

TEST_CASE("publication failure does not confirm the candidate", "[digital_input]")
{
    int level = 1;
    FakePublisher publisher;
    publisher.result = issp::IsspResult::Failed;
    issp::DigitalInputBehavior behavior(makeConfig(), &readLevel, &level);
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(behavior.beginForTest(publisher)));
    feed(behavior, kHighWindow);
    for (std::size_t index = 0; index < kHighWindow.size() - 1; ++index)
    {
        TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                          static_cast<int>(behavior.sampleForTest(kHighWindow[index])));
    }
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Failed),
                      static_cast<int>(behavior.sampleForTest(kHighWindow.back())));
    TEST_ASSERT_FALSE(behavior.hasConfirmedState());
}

TEST_CASE("door commands are recognized and unsupported", "[digital_input]")
{
    int level = 0;
    FakePublisher publisher;
    issp::DigitalInputBehavior behavior(makeConfig(), &readLevel, &level);
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(behavior.beginForTest(publisher)));
    const issp::IsspCommand command{.endpointId = 1, .eventType = 1, .value = 1};
    TEST_ASSERT_TRUE(behavior.accepts(command));
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspCommandResult::Unsupported),
                      static_cast<int>(behavior.handle(command)));
    TEST_ASSERT_EQUAL_size_t(0, publisher.count);
}

TEST_CASE("real timer confirms a stable transition inside the behavior budget",
          "[digital_input][timer]")
{
    int level = 0;
    FakePublisher publisher;
    issp::DigitalInputBehavior behavior(makeConfig(), &readLevel, &level);
    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(behavior.beginTimerForTest(publisher)));
    TEST_ASSERT_EQUAL_size_t(1, publisher.count);

    const std::int64_t transitionStartUs = esp_timer_get_time();
    level = 1;
    vTaskDelay(pdMS_TO_TICKS(170));

    TEST_ASSERT_EQUAL_size_t(2, publisher.count);
    TEST_ASSERT_TRUE((publisher.timestampsUs[1] - transitionStartUs) <= 150000);
}

TEST_CASE("destroying an active timer prevents later callbacks",
          "[digital_input][timer]")
{
    int level = 0;
    FakePublisher publisher;
    {
        issp::DigitalInputBehavior behavior(makeConfig(), &readLevel, &level);
        TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                          static_cast<int>(behavior.beginTimerForTest(publisher)));
        TEST_ASSERT_EQUAL_size_t(1, publisher.count);
    }
    const std::size_t countAfterDestruction = publisher.count;
    vTaskDelay(pdMS_TO_TICKS(30));
    TEST_ASSERT_EQUAL_size_t(countAfterDestruction, publisher.count);
}

extern "C" void app_main()
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
