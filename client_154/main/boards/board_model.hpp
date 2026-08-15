#pragma once

#include <cstdint>

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

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

// A board that also offers the dry_contact_wakeup resource (declared in the
// CMake composition, not here) states that this same pin is eligible as an
// external wakeup source on its target. The physical capability stays with the
// board; using it as a wakeup source stays a product policy.
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

// Electrical facts of a permanently connected battery divider. Chemistry,
// sampling and report policy remain product-firmware responsibilities.
struct BatteryMeasurementResource
{
    adc_unit_t unit;
    adc_channel_t channel;
    adc_atten_t attenuation;
    std::uint32_t rTopOhms;
    std::uint32_t rBottomOhms;
};

// A selected board defines only the accessors for resources it offers. CMake
// rejects an incompatible product/board pair first; these declarations also
// make stale resource metadata fail at link time instead of producing a board
// with invented pins.
const DigitalOutputResource &selectedDigitalOutput();
const UserButtonResource &selectedUserButton();
const DryContactInputResource &selectedDryContactInput();
const WakeLedResource &selectedWakeLed();
const BatteryMeasurementResource &selectedBatteryMeasurement();

} // namespace client154
