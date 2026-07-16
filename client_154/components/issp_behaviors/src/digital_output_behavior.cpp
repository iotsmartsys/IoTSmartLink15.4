#include "digital_output_behavior.hpp"

#include "ibehavior_state_publisher.hpp"

namespace issp
{

DigitalOutputBehavior::DigitalOutputBehavior(const DigitalOutputConfig &config)
    : config_(config),
      publisher_(nullptr),
      state_(config.initialState)
{
}

IsspResult DigitalOutputBehavior::begin(IBehaviorStatePublisher &publisher)
{
    if ((config_.activeLevel != 0U && config_.activeLevel != 1U) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(config_.pin))
    {
        return IsspResult::Failed;
    }

    if (gpio_set_level(config_.pin, levelForState(state_)) != ESP_OK)
    {
        return IsspResult::Failed;
    }

    gpio_config_t gpioConfig{};
    gpioConfig.pin_bit_mask = 1ULL << static_cast<std::uint32_t>(config_.pin);
    gpioConfig.mode = GPIO_MODE_OUTPUT;
    gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
    gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpioConfig.intr_type = GPIO_INTR_DISABLE;

    if (gpio_config(&gpioConfig) != ESP_OK)
    {
        return IsspResult::Failed;
    }

    publisher_ = &publisher;
    return IsspResult::Ok;
}

std::uint32_t DigitalOutputBehavior::levelForState(bool state) const
{
    return state ? config_.activeLevel : (config_.activeLevel == 0U ? 1U : 0U);
}

bool DigitalOutputBehavior::accepts(const IsspCommand &command) const
{
    return command.endpointId == config_.endpointId &&
           command.eventType == config_.eventType;
}

IsspCommandResult DigitalOutputBehavior::handle(const IsspCommand &command)
{
    if (publisher_ == nullptr)
    {
        return IsspCommandResult::Failed;
    }

    if (command.value != 0U && command.value != 1U)
    {
        return IsspCommandResult::Invalid;
    }

    const bool requestedState = command.value == 1U;
    if (gpio_set_level(config_.pin, levelForState(requestedState)) != ESP_OK)
    {
        return IsspCommandResult::Failed;
    }

    state_ = requestedState;

    const IsspReport report{
        .endpointId = config_.endpointId,
        .eventType = config_.eventType,
        .value = static_cast<std::uint8_t>(state_ ? 1U : 0U),
    };

    if (publisher_->publishState(report) != IsspResult::Ok)
    {
        return IsspCommandResult::Failed;
    }

    return IsspCommandResult::Accepted;
}

bool DigitalOutputBehavior::state() const
{
    return state_;
}

} // namespace issp
