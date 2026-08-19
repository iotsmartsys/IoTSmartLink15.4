#include "battery_telemetry_state_behavior.hpp"

#include "esp_log.h"
#include "ibehavior_state_publisher.hpp"

namespace issp
{

namespace
{
constexpr char kTag[] = "BATTERY_STATE";
}

BatteryTelemetryStateBehavior::BatteryTelemetryStateBehavior(
    std::uint8_t endpointId, BatteryLevelBehavior &battery)
    : endpointId_(endpointId),
      battery_(battery),
      publisher_(nullptr),
      hasPublishedState_(false),
      lastPublishedState_(BatteryLevelBehavior::TelemetryState::Inert)
{
    battery_.setTelemetryStateListener(&BatteryTelemetryStateBehavior::telemetryStateChanged,
                                       this);
}

BatteryTelemetryStateBehavior::~BatteryTelemetryStateBehavior()
{
    battery_.setTelemetryStateListener(nullptr, nullptr);
}

IsspResult BatteryTelemetryStateBehavior::begin(IBehaviorStatePublisher &publisher)
{
    if (publisher_ != nullptr || endpointId_ == 0U)
    {
        return IsspResult::InvalidArgument;
    }
    publisher_ = &publisher;
    const IsspResult result = publishIfChanged(battery_.telemetryState());
    if (result != IsspResult::Ok)
    {
        publisher_ = nullptr;
    }
    return result;
}

void BatteryTelemetryStateBehavior::telemetryStateChanged(
    void *context, BatteryLevelBehavior::TelemetryState state)
{
    if (context == nullptr)
    {
        return;
    }
    auto *self = static_cast<BatteryTelemetryStateBehavior *>(context);
    const IsspResult result = self->publishIfChanged(state);
    if (result != IsspResult::Ok)
    {
        ESP_LOGW(kTag, "state_report failed endpoint=%u state=%u result=%u",
                 static_cast<unsigned>(self->endpointId_),
                 static_cast<unsigned>(state), static_cast<unsigned>(result));
    }
}

IsspResult BatteryTelemetryStateBehavior::publishIfChanged(
    BatteryLevelBehavior::TelemetryState state)
{
    if (publisher_ == nullptr)
    {
        return IsspResult::Ok;
    }
    if (hasPublishedState_ && lastPublishedState_ == state)
    {
        return IsspResult::Ok;
    }
    const IsspReport report = {
        .endpointId = endpointId_,
        .eventType = kEventType,
        .value = static_cast<std::uint8_t>(state),
    };
    const IsspResult result = publisher_->publishState(report);
    if (result == IsspResult::Ok)
    {
        hasPublishedState_ = true;
        lastPublishedState_ = state;
    }
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

IsspResult BatteryTelemetryStateBehavior::quiesce()
{
    publisher_ = nullptr;
    return IsspResult::Ok;
}

} // namespace issp
