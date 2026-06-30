#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void iot154_storage_init(void);
bool iot154_storage_load_central_ext_addr(uint8_t *addr);
void iot154_storage_save_central_ext_addr(const uint8_t *addr);
void iot154_storage_reset_pairing(void);

#ifdef __cplusplus
}
#endif
