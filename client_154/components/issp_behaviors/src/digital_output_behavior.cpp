#include "digital_output_behavior.hpp"

#include "esp_log.h"
#include "ibehavior_state_publisher.hpp"

namespace issp
{

namespace
{

constexpr std::uint8_t kOffValue = 0;
constexpr std::uint8_t kOnValue = 1;
constexpr std::uint8_t kToggleValue = 2;

} // namespace

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

    if (config_.reportOnStart)
    {
        const IsspReport initialReport{
            .endpointId = config_.endpointId,
            .eventType = config_.eventType,
            .value = static_cast<std::uint8_t>(state_ ? 1U : 0U),
        };
        const IsspResult publishResult = publisher_->publishState(initialReport);
        ESP_LOGI("DIGITAL_OUTPUT_START",
                 "initial_report endpoint=%u event=%u value=%u result=%u",
                 static_cast<unsigned>(initialReport.endpointId),
                 static_cast<unsigned>(initialReport.eventType),
                 static_cast<unsigned>(initialReport.value),
                 static_cast<unsigned>(publishResult));
        if (publishResult != IsspResult::Ok)
        {
            publisher_ = nullptr;
            return publishResult;
        }
    }

    return IsspResult::Ok;
}

std::uint32_t DigitalOutputBehavior::levelForState(bool state) const
{
    return state ? config_.activeLevel : (config_.activeLevel == 0U ? 1U : 0U);
}

bool DigitalOutputBehavior::accepts(const IsspCommand &command) const
{
    const bool accepted = command.endpointId == config_.endpointId &&
                          command.eventType == config_.eventType;
    ESP_LOGI("DIGITAL_OUTPUT_ACCEPT",
             "endpoint_received=%u endpoint_configured=%u event_received=%u event_configured=%u result=%s",
             static_cast<unsigned>(command.endpointId),
             static_cast<unsigned>(config_.endpointId),
             static_cast<unsigned>(command.eventType),
             static_cast<unsigned>(config_.eventType),
             accepted ? "accepted" : "rejected");
    return accepted;
}

IsspCommandResult DigitalOutputBehavior::handle(const IsspCommand &command)
{
    const bool previousState = state_;
    ESP_LOGI("DIGITAL_OUTPUT_HANDLE",
             "enter value=%u previous_state=%u",
             static_cast<unsigned>(command.value),
             previousState ? 1U : 0U);
    if (publisher_ == nullptr)
    {
        ESP_LOGI("DIGITAL_OUTPUT_HANDLE",
                 "value=rejected reason=no_publisher previous_state=%u resulting_state=%u",
                 previousState ? 1U : 0U,
                 state_ ? 1U : 0U);
        return IsspCommandResult::Failed;
    }

    if (command.value != kOffValue &&
        command.value != kOnValue &&
        command.value != kToggleValue)
    {
        ESP_LOGI("DIGITAL_OUTPUT_HANDLE",
                 "value=rejected value_received=%u previous_state=%u resulting_state=%u",
                 static_cast<unsigned>(command.value),
                 previousState ? 1U : 0U,
                 state_ ? 1U : 0U);
        return IsspCommandResult::Invalid;
    }

    const bool requestedState = command.value == kToggleValue
                                    ? !previousState
                                    : command.value == kOnValue;
    ESP_LOGI("DIGITAL_OUTPUT_HANDLE",
             "value=accepted value_received=%u action=%s requested_state=%u",
             static_cast<unsigned>(command.value),
             command.value == kToggleValue ? "toggle" :
             (command.value == kOnValue ? "on" : "off"),
             requestedState ? 1U : 0U);
    const esp_err_t gpioResult =
        gpio_set_level(config_.pin, levelForState(requestedState));
    ESP_LOGI("DIGITAL_OUTPUT_HANDLE",
             "gpio_set_level_result=%d previous_state=%u resulting_state=%u",
             static_cast<int>(gpioResult),
             previousState ? 1U : 0U,
             gpioResult == ESP_OK ? (requestedState ? 1U : 0U) : (state_ ? 1U : 0U));
    if (gpioResult != ESP_OK)
    {
        return IsspCommandResult::Failed;
    }

    state_ = requestedState;

    const IsspReport report{
        .endpointId = config_.endpointId,
        .eventType = config_.eventType,
        .value = static_cast<std::uint8_t>(state_ ? 1U : 0U),
    };

    const IsspResult publishResult = publisher_->publishState(report);
    ESP_LOGI("DIGITAL_OUTPUT_HANDLE",
             "publish_state_result=%u previous_state=%u resulting_state=%u",
             static_cast<unsigned>(publishResult),
             previousState ? 1U : 0U,
             state_ ? 1U : 0U);
    if (publishResult != IsspResult::Ok)
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
