#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void iot154_boot_button_configure(void);
bool iot154_boot_button_is_pressed(void);

#ifdef __cplusplus
}
#endif
