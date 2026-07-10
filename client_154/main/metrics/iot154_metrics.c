#include "iot154_metrics.h"

#include "esp_attr.h"

#include "iot154_sensor_config.h"

typedef struct {
    uint32_t count;
    uint32_t min_total_ms;
    uint32_t max_total_ms;
    uint32_t last20_total_ms[IOT154_STATS_WINDOW];
    uint8_t last20_pos;
    uint8_t last20_count;
} sensor_stats_t;

RTC_DATA_ATTR static sensor_stats_t s_rtc_stats;

uint32_t iot154_metrics_elapsed_ms(int64_t start_us, int64_t end_us)
{
    if (end_us < start_us) {
        return 0;
    }
    return (uint32_t)((end_us - start_us) / 1000);
}

uint32_t iot154_metrics_update_total_stats(uint32_t total_awake_ms)
{
    if (s_rtc_stats.count == 0 || total_awake_ms < s_rtc_stats.min_total_ms) {
        s_rtc_stats.min_total_ms = total_awake_ms;
    }
    if (s_rtc_stats.count == 0 || total_awake_ms > s_rtc_stats.max_total_ms) {
        s_rtc_stats.max_total_ms = total_awake_ms;
    }

    s_rtc_stats.last20_total_ms[s_rtc_stats.last20_pos] = total_awake_ms;
    s_rtc_stats.last20_pos = (uint8_t)((s_rtc_stats.last20_pos + 1) % IOT154_STATS_WINDOW);
    if (s_rtc_stats.last20_count < IOT154_STATS_WINDOW) {
        ++s_rtc_stats.last20_count;
    }
    ++s_rtc_stats.count;

    uint32_t sum = 0;
    for (uint8_t i = 0; i < s_rtc_stats.last20_count; ++i) {
        sum += s_rtc_stats.last20_total_ms[i];
    }
    return sum / s_rtc_stats.last20_count;
}

uint32_t iot154_metrics_min_total_ms(void)
{
    return s_rtc_stats.min_total_ms;
}

uint32_t iot154_metrics_max_total_ms(void)
{
    return s_rtc_stats.max_total_ms;
}
