#include <array>
#include <cstddef>
#include <cstdint>

#include "digital_input_behavior.hpp"
#include "esp_log.h"
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

// Level source that replays a declared script and then holds a stable tail
// level, so a test can force the synchronous begin() budget to diverge.
struct ScriptedLevels
{
    std::array<std::uint32_t, 10> script{};
    std::size_t index = 0;
    int tail = 0;
};

int readScriptedLevel(void *context)
{
    auto *scripted = static_cast<ScriptedLevels *>(context);
    if (scripted->index < scripted->script.size())
    {
        return static_cast<int>(scripted->script[scripted->index++]);
    }
    return scripted->tail;
}

// Level source that timestamps every sample taken by the real esp_timer, so a
// test can measure the sampling cadence instead of the debounce outcome.
struct SampleClock
{
    std::array<std::int64_t, 32> timestampsUs{};
    std::size_t count = 0;
    int level = 0;
};

int readTimedLevel(void *context)
{
    auto *clock = static_cast<SampleClock *>(context);
    if (clock->count < clock->timestampsUs.size())
    {
        clock->timestampsUs[clock->count] = esp_timer_get_time();
        ++clock->count;
    }
    return clock->level;
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

TEST_CASE("diverging initial classifications keep begin() ok until convergence",
          "[digital_input][timer]")
{
    ScriptedLevels scripted;
    // Two synchronous windows classified high then low: no convergence inside
    // the initial budget. The tail then holds the input low.
    scripted.script = {1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
    scripted.tail = 0;
    FakePublisher publisher;
    issp::DigitalInputBehavior behavior(makeConfig(), &readScriptedLevel, &scripted);

    TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                      static_cast<int>(behavior.beginTimerForTest(publisher)));
    TEST_ASSERT_FALSE(behavior.hasConfirmedState());
    TEST_ASSERT_EQUAL_size_t(0, publisher.count);

    vTaskDelay(pdMS_TO_TICKS(120));

    TEST_ASSERT_TRUE(behavior.hasConfirmedState());
    TEST_ASSERT_FALSE(behavior.state());
    TEST_ASSERT_EQUAL_size_t(1, publisher.count);
    TEST_ASSERT_EQUAL_UINT8(0, publisher.reports[0].value);
}

TEST_CASE("the periodic timer samples at the configured 10 ms cadence",
          "[digital_input][timer]")
{
    SampleClock clock;
    FakePublisher publisher;
    {
        issp::DigitalInputBehavior behavior(makeConfig(), &readTimedLevel, &clock);
        TEST_ASSERT_EQUAL(static_cast<int>(issp::IsspResult::Ok),
                          static_cast<int>(behavior.beginTimerForTest(publisher)));
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    // The behavior is destroyed before the samples are read, so the timer is
    // stopped and the recorded window is stable.
    const std::size_t synchronousSamples = 10;
    TEST_ASSERT_TRUE(clock.count >= synchronousSamples + 11);

    std::int64_t maxIntervalUs = 0;
    for (std::size_t index = synchronousSamples + 1; index < clock.count; ++index)
    {
        const std::int64_t interval =
            clock.timestampsUs[index] - clock.timestampsUs[index - 1];
        if (interval > maxIntervalUs)
        {
            maxIntervalUs = interval;
        }
    }
    const std::size_t intervals = clock.count - synchronousSamples - 1;
    const std::int64_t meanIntervalUs =
        (clock.timestampsUs[clock.count - 1] -
         clock.timestampsUs[synchronousSamples]) /
        static_cast<std::int64_t>(intervals);

    // The largest interval is recorded to make discarded events visible; it is
    // not an oracle of the debounce, which is declared per sample elsewhere.
    ESP_LOGI("digital_input_test",
             "cadence intervals=%u mean_us=%lld max_us=%lld",
             static_cast<unsigned>(intervals), meanIntervalUs, maxIntervalUs);
    TEST_ASSERT_TRUE(meanIntervalUs >= 9000);
    TEST_ASSERT_TRUE(meanIntervalUs <= 11000);
}

extern "C" void app_main()
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
