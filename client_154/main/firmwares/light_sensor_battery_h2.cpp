#include <cstdint>

#include "boards/board_model.hpp"
#include "product_firmware.hpp"
#include "sdkconfig.h"

using namespace iotsmartsys;

namespace
{
constexpr std::uint32_t kDeviceId = 0x15400003;
constexpr std::uint8_t kLightEndpointId = 1;
constexpr std::uint32_t kMaxAwakeTimeMs =
    static_cast<std::uint32_t>(CONFIG_IOTSMARTLINK154_MAX_AWAKE_TIME_SECONDS) * 1000U;
constexpr std::uint32_t kSleepIntervalMinutes =
    static_cast<std::uint32_t>(CONFIG_IOTSMARTLINK154_WAKEUP_INTERVAL_MINUTES);

#if CONFIG_IOTSMARTLINK154_ENABLE_BATTERY_LEVEL
constexpr std::uint8_t kBatteryEndpointId = 2;
constexpr std::uint32_t kBatteryEmptyMv = 3300;
constexpr std::uint32_t kBatteryFullMv = 4150;
constexpr std::uint32_t kBatterySamples = 8;
constexpr std::uint32_t kBatterySampleIntervalMs = 5;
constexpr std::uint8_t kBatteryReportDeltaPercent = 5;
#endif

SmartSysApp smartSysApp({.deviceId = kDeviceId});
}

namespace client154
{

iotsmartsys::SetupResult startSelectedProductFirmware()
{
    // Light releases ADC1 in begin(); battery retains it until quiescence.
    // Registration order lets both channels acquire during this boot.
    const LightMeasurementResource &light = selectedLightMeasurement();
    smartSysApp.addLightSensorCapability({
        .unit = light.unit,
        .channel = light.channel,
        .attenuation = light.attenuation,
        .endpointId = kLightEndpointId,
    });
#if CONFIG_IOTSMARTLINK154_ENABLE_BATTERY_LEVEL
    const BatteryMeasurementResource &battery = selectedBatteryMeasurement();
    smartSysApp.addBatteryLevelCapability({
        .unit = battery.unit,
        .channel = battery.channel,
        .attenuation = battery.attenuation,
        .rTopOhms = battery.rTopOhms,
        .rBottomOhms = battery.rBottomOhms,
        .emptyMv = kBatteryEmptyMv,
        .fullMv = kBatteryFullMv,
        .samples = kBatterySamples,
        .sampleIntervalMs = kBatterySampleIntervalMs,
        // This product always sleeps, regardless of the digital product opt-in.
        .samplePeriodMs = 0U,
        .reportDeltaPercent = kBatteryReportDeltaPercent,
        .endpointId = kBatteryEndpointId,
    });
#endif
    smartSysApp.configureDeepSleep({
        .enabled = true,
        .maxAwakeTimeMs = kMaxAwakeTimeMs,
        .timerWakeup = {
            .enabled = true,
            .interval = kSleepIntervalMinutes,
            .unit = app::DeepSleepTimeUnit::Minutes,
        },
        .wakeLed = {},
    });
    return smartSysApp.setup();
}

} // namespace client154
