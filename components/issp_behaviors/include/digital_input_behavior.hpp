#pragma once

#include <cstdint>

#include "driver/gpio.h"
#include "esp_timer.h"
#include "idevice_behavior.hpp"

namespace issp
{

enum class DigitalInputPull : std::uint8_t
{
    Floating,
    PullUp,
    PullDown,
};

struct DigitalInputConfig
{
    std::uint8_t endpointId;
    std::uint8_t eventType;
    gpio_num_t pin;
    std::uint32_t activeLevel;
    DigitalInputPull pull;
    bool reportOnStart;
    std::uint32_t samplePeriodMs;
    std::uint8_t samplesPerWindow;
    std::uint8_t majorityThreshold;
    std::uint8_t consecutiveWindows;
};

class DigitalInputBehavior final : public IDeviceBehavior
{
public:
    using LevelReader = int (*)(void *context);

    explicit DigitalInputBehavior(const DigitalInputConfig &config);

    // Test-only seam: beginForTest() skips GPIO and timer setup, and
    // sampleForTest() lets a test declare every sampled level deterministically.
    DigitalInputBehavior(const DigitalInputConfig &config,
                         LevelReader levelReader,
                         void *levelReaderContext);

    ~DigitalInputBehavior() override;

    DigitalInputBehavior(const DigitalInputBehavior &) = delete;
    DigitalInputBehavior &operator=(const DigitalInputBehavior &) = delete;
    DigitalInputBehavior(DigitalInputBehavior &&) = delete;
    DigitalInputBehavior &operator=(DigitalInputBehavior &&) = delete;

    IsspResult begin(IBehaviorStatePublisher &publisher) override;
    bool accepts(const IsspCommand &command) const override;
    IsspCommandResult handle(const IsspCommand &command) override;

    IsspResult beginForTest(IBehaviorStatePublisher &publisher);
    IsspResult beginTimerForTest(IBehaviorStatePublisher &publisher);
    IsspResult sampleForTest(std::uint32_t level);
    bool hasConfirmedState() const;
    bool state() const;

private:
    static void timerCallback(void *context);

    bool validConfig() const;
    IsspResult configureGpio();
    IsspResult beginTimerBacked(IBehaviorStatePublisher &publisher);
    IsspResult createTimer();
    void stopAndDeleteTimer();
    IsspResult sampleCurrentLevel();
    IsspResult processSample(std::uint32_t level);
    IsspResult publishConfirmedState(bool state, bool initial);

    DigitalInputConfig config_;
    IBehaviorStatePublisher *publisher_;
    LevelReader levelReader_;
    void *levelReaderContext_;
    esp_timer_handle_t timer_;
    bool timerStarted_;
    std::uint8_t sampleCount_;
    std::uint8_t activeSampleCount_;
    bool hasWindowClassification_;
    bool lastWindowClassification_;
    std::uint8_t consecutiveClassificationCount_;
    bool hasConfirmedState_;
    bool confirmedState_;
};

} // namespace issp
