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

SmartSysApp smartSysApp({.deviceId = kDeviceId});
}

namespace client154
{

iotsmartsys::SetupResult startSelectedProductFirmware()
{
    const LightMeasurementResource &light = selectedLightMeasurement();
    smartSysApp.addLightSensorCapability({
        .unit = light.unit,
        .channel = light.channel,
        .attenuation = light.attenuation,
        .endpointId = kLightEndpointId,
    });
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
