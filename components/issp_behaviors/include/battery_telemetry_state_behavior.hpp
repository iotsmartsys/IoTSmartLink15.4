#pragma once

#include <cstdint>

#include "battery_level_behavior.hpp"
#include "idevice_behavior.hpp"

namespace issp
{

class BatteryTelemetryStateBehavior final : public IDeviceBehavior
{
public:
    static constexpr std::uint8_t kEventType = 4;

    BatteryTelemetryStateBehavior(std::uint8_t endpointId,
                                   BatteryLevelBehavior &battery);
    ~BatteryTelemetryStateBehavior() override;

    IsspResult begin(IBehaviorStatePublisher &publisher) override;
    bool accepts(const IsspCommand &command) const override;
    IsspCommandResult handle(const IsspCommand &command) override;
    IsspResult quiesce() override;

private:
    static void telemetryStateChanged(void *context,
                                      BatteryLevelBehavior::TelemetryState state);
    IsspResult publishIfChanged(BatteryLevelBehavior::TelemetryState state);

    std::uint8_t endpointId_;
    BatteryLevelBehavior &battery_;
    IBehaviorStatePublisher *publisher_;
    bool hasPublishedState_;
    BatteryLevelBehavior::TelemetryState lastPublishedState_;
};

} // namespace issp
