#pragma once

#include <cstdint>

#include "driver/gpio.h"

namespace client154
{

struct DigitalOutputResource
{
    gpio_num_t pin;
    bool activeHigh;
};

struct UserButtonResource
{
    gpio_num_t pin;
    bool activeLow;
};

enum class InputPull : std::uint8_t
{
    Floating,
    PullUp,
    PullDown,
};

struct DryContactInputResource
{
    gpio_num_t pin;
    bool activeHigh;
    InputPull pull;
};

// A selected board defines only the accessors for resources it offers. CMake
// rejects an incompatible product/board pair first; these declarations also
// make stale resource metadata fail at link time instead of producing a board
// with invented pins.
const DigitalOutputResource &selectedDigitalOutput();
const UserButtonResource &selectedUserButton();
const DryContactInputResource &selectedDryContactInput();

} // namespace client154
