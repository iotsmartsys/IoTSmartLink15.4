#include "battery_telemetry_state_behavior.hpp"

#include "ibehavior_state_publisher.hpp"

namespace issp
{

BatteryTelemetryStateBehavior::BatteryTelemetryStateBehavior(
    std::uint8_t endpointId, const BatteryLevelBehavior &battery)
    : endpointId_(endpointId), battery_(battery), publisher_(nullptr)
{
}

IsspResult BatteryTelemetryStateBehavior::begin(IBehaviorStatePublisher &publisher)
{
    if (publisher_ != nullptr || endpointId_ == 0U)
    {
        return IsspResult::InvalidArgument;
    }
    publisher_ = &publisher;
    const IsspReport report = {
        .endpointId = endpointId_,
        .eventType = kEventType,
        .value = static_cast<std::uint8_t>(battery_.telemetryState()),
    };
    const IsspResult result = publisher_->publishState(report);
    return result;
}

bool BatteryTelemetryStateBehavior::accepts(const IsspCommand &command) const
{
    return command.endpointId == endpointId_ && command.eventType == kEventType;
}

IsspCommandResult BatteryTelemetryStateBehavior::handle(const IsspCommand &command)
{
    (void)command;
    return IsspCommandResult::Unsupported;
}

} // namespace issp
