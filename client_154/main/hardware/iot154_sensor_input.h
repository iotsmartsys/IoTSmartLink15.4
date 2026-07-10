#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void iot154_sensor_input_configure(void);
uint8_t iot154_sensor_input_sample_level(void);
uint8_t iot154_sensor_input_data_value(uint8_t gpio_level);

#ifdef __cplusplus
}
#endif
