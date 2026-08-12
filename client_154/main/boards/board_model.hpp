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

// Indicator lit on every operational boot of a battery-powered product. The
// board owns only the GPIO and the electrical polarity; the duration and the
// on mode are product policy.
struct WakeLedResource
{
    gpio_num_t pin;
    bool activeHigh;
};

// A selected board defines only the accessors for resources it offers. CMake
// rejects an incompatible product/board pair first; these declarations also
// make stale resource metadata fail at link time instead of producing a board
// with invented pins.
const DigitalOutputResource &selectedDigitalOutput();
const UserButtonResource &selectedUserButton();
const DryContactInputResource &selectedDryContactInput();
const WakeLedResource &selectedWakeLed();

} // namespace client154
