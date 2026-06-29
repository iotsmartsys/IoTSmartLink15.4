#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "iot154_metrics.h"

#ifdef __cplusplus
extern "C" {
#endif

void iot154_power_enter_deep_sleep(uint16_t seq,
                                   uint8_t gpio_level,
                                   uint8_t attempts,
                                   bool delivered,
                                   const iot154_sensor_metrics_t *metrics);

#ifdef __cplusplus
}
#endif
