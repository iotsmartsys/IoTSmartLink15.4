#pragma once

#include <cstdint>

#include "driver/gpio.h"
#include "idevice_behavior.hpp"

namespace issp
{

struct DigitalOutputConfig
{
    std::uint8_t endpointId;
    std::uint8_t eventType;
    gpio_num_t pin;
    std::uint32_t activeLevel;
    bool initialState;
    bool reportOnStart;
};

class DigitalOutputBehavior final : public IDeviceBehavior
{
public:
    explicit DigitalOutputBehavior(const DigitalOutputConfig &config);

    IsspResult begin(IBehaviorStatePublisher &publisher) override;
    bool accepts(const IsspCommand &command) const override;
    IsspCommandResult handle(const IsspCommand &command) override;
    bool state() const;

private:
    std::uint32_t levelForState(bool state) const;

    DigitalOutputConfig config_;
    IBehaviorStatePublisher *publisher_;
    bool state_;
};

} // namespace issp
