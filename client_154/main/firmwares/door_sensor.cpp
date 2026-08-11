#include <cstdint>

#include "boards/board_model.hpp"
#include "product_firmware.hpp"

using namespace iotsmartsys;

namespace
{
constexpr std::uint32_t kDeviceId = 0x15400001;
constexpr std::uint8_t kDoorEndpointId = 1;
constexpr std::uint8_t kDoorEventType = 1;
constexpr bool kReportOnStart = true;
constexpr std::uint32_t kSamplePeriodMs = 10;
constexpr std::uint8_t kSamplesPerWindow = 5;
constexpr std::uint8_t kMajorityThreshold = 3;
constexpr std::uint8_t kConsecutiveWindows = 2;
constexpr std::uint32_t kFactoryResetHoldTimeMs = 10000;
constexpr std::uint32_t kFactoryResetPollIntervalMs = 20;

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

    smartSysApp.addDoorSensorCapability({
        .pin = input.pin,
        .activeHigh = input.activeHigh,
        .pull = mapPull(input.pull),
        .reportOnStart = kReportOnStart,
        .endpointId = kDoorEndpointId,
        .eventType = kDoorEventType,
        .samplePeriodMs = kSamplePeriodMs,
        .samplesPerWindow = kSamplesPerWindow,
        .majorityThreshold = kMajorityThreshold,
        .consecutiveWindows = kConsecutiveWindows,
    });

    smartSysApp.configureFactoryResetButton({
        .pin = button.pin,
        .activeLow = button.activeLow,
        .holdTimeMs = kFactoryResetHoldTimeMs,
        .pollIntervalMs = kFactoryResetPollIntervalMs,
    });

    return smartSysApp.setup();
}

} // namespace client154
