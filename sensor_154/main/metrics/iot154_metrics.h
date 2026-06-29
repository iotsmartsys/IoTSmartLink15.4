#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int64_t boot_us;
    int64_t gpio_sampled_us;
    int64_t radio_init_done_us;
    int64_t tx_start_us;
    int64_t ack_received_us;
    int64_t sleep_enter_us;
} iot154_sensor_metrics_t;

uint32_t iot154_metrics_elapsed_ms(int64_t start_us, int64_t end_us);
uint32_t iot154_metrics_update_total_stats(uint32_t total_awake_ms);
uint32_t iot154_metrics_min_total_ms(void);
uint32_t iot154_metrics_max_total_ms(void);

#ifdef __cplusplus
}
#endif
