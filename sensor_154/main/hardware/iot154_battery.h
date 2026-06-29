#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void iot154_battery_init(void);
uint16_t iot154_battery_read_mv(void);
uint8_t iot154_battery_percent_from_mv(uint16_t battery_mv);

#ifdef __cplusplus
}
#endif
