#include <cstdint>

#include "boards/board_model.hpp"
#include "product_firmware.hpp"

// The instance below is named "smartSysApp", not "app": with
// "using namespace iotsmartsys;" in scope, an "app" identifier would be
// ambiguous with the nested "iotsmartsys::app" namespace.
using namespace iotsmartsys;

namespace
{
constexpr std::uint32_t kDeviceId = 0x15400001;
constexpr std::uint8_t kRelayEndpointId = 1;
constexpr std::uint8_t kPowerEventType = 2;
constexpr bool kRelayInitialState = false;
constexpr bool kRelayReportOnStart = true;
constexpr std::uint32_t kFactoryResetHoldTimeMs = 10000;
constexpr std::uint32_t kFactoryResetPollIntervalMs = 20;

SmartSysApp smartSysApp({
    .deviceId = kDeviceId,
});
}

namespace client154
{

iotsmartsys::SetupResult startSelectedProductFirmware()
{
    const BoardModel &board = selectedBoard();

    smartSysApp.addSwitchPlugCapability({
        .pin = board.relayPin,
        .activeHigh = board.relayActiveHigh,
        .initialState = kRelayInitialState,
        .reportOnStart = kRelayReportOnStart,
        .endpointId = kRelayEndpointId,
        .eventType = kPowerEventType,
    });

    smartSysApp.configureFactoryResetButton({
        .pin = board.factoryResetButtonPin,
        .activeLow = board.factoryResetButtonActiveLow,
        .holdTimeMs = kFactoryResetHoldTimeMs,
        .pollIntervalMs = kFactoryResetPollIntervalMs,
    });

    return smartSysApp.setup();
}

} // namespace client154
