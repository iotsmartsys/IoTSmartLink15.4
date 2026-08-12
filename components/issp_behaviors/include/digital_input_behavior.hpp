#pragma once

#include <atomic>
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
    /// Stops and deletes the sampling timer without destroying the object and
    /// without publishing another report. Terminal and idempotent in the boot;
    /// a no-op returning Ok when begin() never completed successfully.
    IsspResult quiesce() override;

    /// Production query: true only after a required initial publication has been
    /// admitted, which is what makes it usable as positive evidence of the
    /// initial report.
    bool hasConfirmedState() const;
    bool state() const;

    // Test-only seams: beginForTest() and beginTimerForTest() install a
    // publisher without the production GPIO path, and sampleForTest() lets a
    // test declare every sampled level deterministically. They are never used by
    // production firmware.
    IsspResult beginForTest(IBehaviorStatePublisher &publisher);
    IsspResult beginTimerForTest(IBehaviorStatePublisher &publisher);
    IsspResult sampleForTest(std::uint32_t level);

private:
    // Single atomic word so the esp_timer task and any reader task observe a
    // coherent unknown/inactive/active state without a data race.
    static constexpr std::uint8_t kStateUnknown = 0;
    static constexpr std::uint8_t kStateInactive = 1;
    static constexpr std::uint8_t kStateActive = 2;

    static void timerCallback(void *context);

    bool validConfig() const;
    IsspResult configureGpio();
    IsspResult beginTimerBacked(IBehaviorStatePublisher &publisher);
    IsspResult createTimer();
    void stopAndDeleteTimer();
    IsspResult readLevel(std::uint32_t &level) const;
    IsspResult sampleCurrentLevel();
    IsspResult processSample(std::uint32_t level);
    void trackDivergence(std::uint32_t level);
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
    bool hasDivergenceStart_;
    std::int64_t divergenceStartUs_;
    std::atomic<std::uint8_t> confirmedState_;
};

} // namespace issp
