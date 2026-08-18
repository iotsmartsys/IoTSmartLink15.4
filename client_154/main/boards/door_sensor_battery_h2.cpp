#include "boards/board_model.hpp"
#include "sdkconfig.h"
#include "soc/adc_channel.h"

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
    .pin = static_cast<gpio_num_t>(CONFIG_IOTSMARTLINK154_FACTORY_RESET_GPIO),
    .activeLow = true,
};
constexpr WakeLedResource kWakeLed = {
    .pin = GPIO_NUM_13,
    .activeHigh = true,
};
constexpr BatteryMeasurementResource kBatteryMeasurement = {
    .unit = ADC_UNIT_1,
    .channel = ADC_CHANNEL_0,
    .attenuation = ADC_ATTEN_DB_12,
    .rTopOhms = 470000U,
    .rBottomOhms = 220000U,
};
static_assert(kUserButton.pin != kDryContactInput.pin,
              "App Client composition rejected: factory reset GPIO collides "
              "with dry_contact_input");
#if CONFIG_IOTSMARTLINK154_ENABLE_DEEP_SLEEP
static_assert(kUserButton.pin != kWakeLed.pin,
              "App Client composition rejected: factory reset GPIO collides "
              "with wake_led");
#endif
#if CONFIG_IOTSMARTLINK154_ENABLE_BATTERY_LEVEL
static_assert(kUserButton.pin !=
                  static_cast<gpio_num_t>(ADC1_CHANNEL_0_GPIO_NUM),
              "App Client composition rejected: factory reset GPIO collides "
              "with battery_measurement");
#endif
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

const BatteryMeasurementResource &selectedBatteryMeasurement()
{
    return kBatteryMeasurement;
}

} // namespace client154
