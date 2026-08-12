#include "boards/board_model.hpp"
#include "sdkconfig.h"

#ifndef CONFIG_IDF_TARGET_ESP32H2
#error "Board model 'Door Sensor Battery H2' supports only IDF_TARGET=esp32h2."
#endif

namespace client154
{
namespace
{
// GPIO 14 is inside the range the ESP32-H2 accepts as an external wakeup
// source, which is what lets this board offer the dry_contact_wakeup resource.
constexpr DryContactInputResource kDryContactInput = {
    .pin = GPIO_NUM_14,
    .activeHigh = true,
    .pull = InputPull::PullUp,
};
constexpr UserButtonResource kUserButton = {
    .pin = GPIO_NUM_9,
    .activeLow = true,
};
constexpr WakeLedResource kWakeLed = {
    .pin = GPIO_NUM_13,
    .activeHigh = true,
};
}

const DryContactInputResource &selectedDryContactInput()
{
    return kDryContactInput;
}

const UserButtonResource &selectedUserButton()
{
    return kUserButton;
}

const WakeLedResource &selectedWakeLed()
{
    return kWakeLed;
}

} // namespace client154
