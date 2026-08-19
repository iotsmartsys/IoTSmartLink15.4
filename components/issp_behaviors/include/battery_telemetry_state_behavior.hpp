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
                                   const BatteryLevelBehavior &battery);

    IsspResult begin(IBehaviorStatePublisher &publisher) override;
    bool accepts(const IsspCommand &command) const override;
    IsspCommandResult handle(const IsspCommand &command) override;

private:
    std::uint8_t endpointId_;
    const BatteryLevelBehavior &battery_;
    IBehaviorStatePublisher *publisher_;
};

} // namespace issp
