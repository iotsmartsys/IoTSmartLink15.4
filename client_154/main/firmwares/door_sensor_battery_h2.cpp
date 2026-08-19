#include <cstdint>

#include "boards/board_model.hpp"
#include "product_firmware.hpp"

using namespace iotsmartsys;

namespace
{
constexpr std::uint32_t kDeviceId = 0x15400001;
constexpr std::uint8_t kDoorEndpointId = 1;
constexpr std::uint8_t kBatteryEndpointId = 2;
constexpr bool kReportOnStart = true;
constexpr std::uint32_t kSamplePeriodMs = 10;
constexpr std::uint8_t kSamplesPerWindow = 5;
constexpr std::uint8_t kMajorityThreshold = 3;
constexpr std::uint8_t kConsecutiveWindows = 2;
constexpr std::uint32_t kFactoryResetHoldTimeMs = 10000;
constexpr std::uint32_t kFactoryResetPollIntervalMs = 20;
// Battery policy of this product: how long the device may stay awake, how long
// it sleeps and how the indicator behaves. The board owns only the LED GPIO and
// its electrical polarity.
constexpr std::uint32_t kMaxAwakeTimeMs =
#if CONFIG_IOTSMARTLINK154_ENABLE_DEEP_SLEEP
    static_cast<std::uint32_t>(CONFIG_IOTSMARTLINK154_MAX_AWAKE_TIME_SECONDS) * 1000U;
constexpr std::uint32_t kSleepInterval =
    static_cast<std::uint32_t>(CONFIG_IOTSMARTLINK154_WAKEUP_INTERVAL_MINUTES);
#else
    0U;
constexpr std::uint32_t kSleepInterval = 0U;
#endif
constexpr app::DeepSleepTimeUnit kSleepIntervalUnit = app::DeepSleepTimeUnit::Minutes;
constexpr std::uint32_t kWakeLedOnTimeMs = 200;
// Battery chemistry and reporting policy. ADC wiring and divider values come
// from the selected board model below.
constexpr std::uint32_t kBatteryEmptyMv = 3300;
constexpr std::uint32_t kBatteryFullMv = 4150;
constexpr std::uint32_t kBatterySamples = 8;
constexpr std::uint32_t kBatterySampleIntervalMs = 5;
constexpr std::uint32_t kBatterySamplePeriodMs =
#if CONFIG_IOTSMARTLINK154_ENABLE_BATTERY_LEVEL && !CONFIG_IOTSMARTLINK154_ENABLE_DEEP_SLEEP
    static_cast<std::uint32_t>(CONFIG_IOTSMARTLINK154_BATTERY_READING_INTERVAL_MINUTES) *
        60000U;
#else
    0U;
#endif
constexpr std::uint8_t kBatteryReportDeltaPercent = 5;

SmartSysApp smartSysApp({
    .deviceId = kDeviceId,
});

app::DigitalInputPull mapPull(client154::InputPull pull)
{
    switch (pull)
    {
    case client154::InputPull::Floating:
        return app::DigitalInputPull::Floating;
    case client154::InputPull::PullUp:
        return app::DigitalInputPull::PullUp;
    case client154::InputPull::PullDown:
        return app::DigitalInputPull::PullDown;
    }
    return app::DigitalInputPull::Floating;
}
}

namespace client154
{

iotsmartsys::SetupResult startSelectedProductFirmware()
{
    const DryContactInputResource &input = selectedDryContactInput();
    const UserButtonResource &button = selectedUserButton();
#if CONFIG_IOTSMARTLINK154_ENABLE_DEEP_SLEEP
    const WakeLedResource &wakeLed = selectedWakeLed();
#endif
#if CONFIG_IOTSMARTLINK154_ENABLE_BATTERY_LEVEL
    const BatteryMeasurementResource &battery = selectedBatteryMeasurement();
#endif

    smartSysApp.addDoorSensorCapability({
        .pin = input.pin,
        .activeHigh = input.activeHigh,
        .pull = mapPull(input.pull),
        .reportOnStart = kReportOnStart,
        .endpointId = kDoorEndpointId,
        .samplePeriodMs = kSamplePeriodMs,
        .samplesPerWindow = kSamplesPerWindow,
        .majorityThreshold = kMajorityThreshold,
        .consecutiveWindows = kConsecutiveWindows,
    });

#if CONFIG_IOTSMARTLINK154_ENABLE_BATTERY_LEVEL
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
        .samplePeriodMs = kBatterySamplePeriodMs,
        .reportDeltaPercent = kBatteryReportDeltaPercent,
        .endpointId = kBatteryEndpointId,
    });
#endif

    smartSysApp.configureFactoryResetButton({
        .pin = button.pin,
        .activeLow = button.activeLow,
        .holdTimeMs = kFactoryResetHoldTimeMs,
        .pollIntervalMs = kFactoryResetPollIntervalMs,
    });

#if CONFIG_IOTSMARTLINK154_ENABLE_DEEP_SLEEP
    smartSysApp.configureDeepSleep({
        .enabled = true,
        .maxAwakeTimeMs = kMaxAwakeTimeMs,
        .timerWakeup = {
            .enabled = true,
            .interval = kSleepInterval,
            .unit = kSleepIntervalUnit,
        },
        .wakeLed = {
            .enabled = true,
            .pin = wakeLed.pin,
            .activeHigh = wakeLed.activeHigh,
            .onMode = app::WakeLedOnMode::DurationMs,
            .onTimeMs = kWakeLedOnTimeMs,
        },
        // The timer is the periodic sign of life; the contact is the event, so
        // a transition is reported when it happens and not only at the next
        // period. The GPIO is the one the board offers as the dry-contact
        // input, which is also the capability registered above.
        .contactWakeup = {
            .enabled = true,
            .pin = input.pin,
        },
    });
#endif

    return smartSysApp.setup();
}

} // namespace client154
