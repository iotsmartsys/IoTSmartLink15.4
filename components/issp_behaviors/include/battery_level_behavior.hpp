#pragma once

#include <cstdint>

#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "idevice_behavior.hpp"

namespace issp
{

struct BatteryLevelConfig
{
    adc_unit_t unit;
    adc_channel_t channel;
    adc_atten_t attenuation;
    std::uint32_t rTopOhms;
    std::uint32_t rBottomOhms;
    std::uint32_t emptyMv;
    std::uint32_t fullMv;
    std::uint32_t samples;
    std::uint32_t sampleIntervalMs;
    std::uint32_t samplePeriodMs;
    std::uint8_t reportDeltaPercent;
    std::uint8_t endpointId;
};

class BatteryLevelBehavior final : public IDeviceBehavior
{
public:
    static constexpr std::uint8_t kEventType = 3;

    explicit BatteryLevelBehavior(const BatteryLevelConfig &config);
    ~BatteryLevelBehavior() override;

    BatteryLevelBehavior(const BatteryLevelBehavior &) = delete;
    BatteryLevelBehavior &operator=(const BatteryLevelBehavior &) = delete;
    BatteryLevelBehavior(BatteryLevelBehavior &&) = delete;
    BatteryLevelBehavior &operator=(BatteryLevelBehavior &&) = delete;

    IsspResult begin(IBehaviorStatePublisher &publisher) override;
    IsspResult startDeferredSampling();
    bool accepts(const IsspCommand &command) const override;
    IsspCommandResult handle(const IsspCommand &command) override;
    IsspResult quiesce() override;

private:
    static constexpr adc_bitwidth_t kBitwidth = ADC_BITWIDTH_12;
    static constexpr int kMaximumRaw = (1 << 12) - 1;

    static void timerCallback(void *context);

    bool validConfig() const;
    bool initializeAdc();
    void releaseAdc();
    IsspResult createAndStartTimer();
    void stopAndDeleteTimer();
    IsspResult measureAndMaybePublish();
    bool readPinMillivolts(std::uint32_t &pinMv);
    std::uint32_t fallbackFullScaleMv() const;
    std::uint8_t percentageFromBatteryMv(std::uint64_t batteryMv) const;

    BatteryLevelConfig config_;
    IBehaviorStatePublisher *publisher_;
    adc_oneshot_unit_handle_t adcUnit_;
    adc_cali_handle_t calibration_;
    esp_timer_handle_t timer_;
    bool timerStarted_;
    bool inert_;
    bool hasPublishedPercentage_;
    std::uint8_t lastPublishedPercentage_;
};

} // namespace issp
