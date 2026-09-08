#pragma once

#include <atomic>
#include <cstdint>

#include "esp_adc/adc_oneshot.h"
#include "idevice_behavior.hpp"

namespace issp
{

struct LightSensorConfig
{
    adc_unit_t unit;
    adc_channel_t channel;
    adc_atten_t attenuation;
    std::uint8_t endpointId;
};

// One synchronous acquisition per operational boot. No timer, calibration,
// retained sample or autonomous retry; the report executor owns retransmission.
class LightSensorBehavior final : public IDeviceBehavior
{
public:
    static constexpr std::uint8_t kEventType = 6;

    explicit LightSensorBehavior(const LightSensorConfig &config);
    IsspResult begin(IBehaviorStatePublisher &publisher) override;
    bool accepts(const IsspCommand &command) const override;
    IsspCommandResult handle(const IsspCommand &command) override;
    IsspResult quiesce() override;
    bool hasAdmittedReport() const;

private:
    LightSensorConfig config_;
    std::atomic<bool> started_{false};
    std::atomic<bool> quiesced_{false};
    std::atomic<bool> admitted_{false};
};

} // namespace issp
