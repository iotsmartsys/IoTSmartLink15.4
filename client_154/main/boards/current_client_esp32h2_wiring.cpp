#include "boards/board_model.hpp"
#include "sdkconfig.h"

// Descriptive identifier for the wiring currently used by the client_154
// prototype. It is not a commercial board name: renaming it depends on the
// Architect confirming the real board name and revision.
//
// main/CMakeLists.txt already refuses to configure this board for another
// target, because its Kconfig option depends on IDF_TARGET_ESP32H2. This
// guard keeps the incompatibility detectable if the file is ever compiled
// through another path.
#ifndef CONFIG_IDF_TARGET_ESP32H2
#error "Board model 'Current client ESP32-H2 wiring' supports only IDF_TARGET=esp32h2."
#endif

namespace client154
{
namespace
{
constexpr DigitalOutputResource kDigitalOutput = {
    .pin = GPIO_NUM_13,
    .activeHigh = true,
};
constexpr UserButtonResource kUserButton = {
    .pin = static_cast<gpio_num_t>(CONFIG_IOTSMARTLINK154_FACTORY_RESET_GPIO),
    .activeLow = true,
};
static_assert(kUserButton.pin != kDigitalOutput.pin,
              "App Client composition rejected: factory reset GPIO collides "
              "with digital_output");
}

const DigitalOutputResource &selectedDigitalOutput()
{
    return kDigitalOutput;
}

const UserButtonResource &selectedUserButton()
{
    return kUserButton;
}

} // namespace client154
