#include "boards/board_model.hpp"
#include "sdkconfig.h"
#include "soc/adc_channel.h"

#ifndef CONFIG_IDF_TARGET_ESP32H2
#error "Board model 'Light Sensor H2' supports only IDF_TARGET=esp32h2."
#endif

namespace client154
{
namespace
{
// 3.3 V -> LDR -> GPIO2 -> 10 kohm -> GND. No calibrated light extremes.
constexpr LightMeasurementResource kLightMeasurement = {
    .unit = ADC_UNIT_1,
    .channel = ADC_CHANNEL_1,
    .attenuation = ADC_ATTEN_DB_12,
    .supplyMv = 3300U,
    .resistorOhms = 10000U,
};
static_assert(ADC1_CHANNEL_1_GPIO_NUM == GPIO_NUM_2,
              "Light Sensor H2 requires ADC1 channel 1 on GPIO2");
}

const LightMeasurementResource &selectedLightMeasurement()
{
    return kLightMeasurement;
}

} // namespace client154
