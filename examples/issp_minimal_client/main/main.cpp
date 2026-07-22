#include <cstdint>

#include "digital_output_behavior.hpp"
#include "issp_device.hpp"
#include "issp154_network_manager.hpp"
#include "issp154_report_executor.hpp"
#include "issp154_transport.hpp"

namespace
{
constexpr std::uint32_t kExampleDeviceId = 0x15400002;
constexpr std::uint8_t kExampleEndpointId = 1;
constexpr std::uint8_t kExampleEventType = 2;
}

extern "C" void app_main()
{
    static std::uint8_t extendedAddress[issp::kIssp154ExtendedAddressSize] = {};
    static const issp::Issp154TransportConfig transportConfig = {
        .channel = 0,
        .panId = 0,
        .shortAddress = 0,
        .coordinator = false,
        .extendedAddress = extendedAddress,
        .cca = true,
        .promiscuous = false,
    };
    static const issp::IsspDeviceConfig deviceConfig = {
        .deviceId = kExampleDeviceId,
    };
    static const issp::DigitalOutputConfig behaviorConfig = {
        .endpointId = kExampleEndpointId,
        .eventType = kExampleEventType,
        .pin = GPIO_NUM_NC,
        .activeLevel = 1,
        .initialState = false,
        .reportOnStart = false,
    };

    // Constructing the public types proves compile and link integration. The
    // example deliberately does not call begin(), start(), initializeNetwork()
    // or any persistence API, so running it has no radio or NVS side effects.
    static issp::Issp154Transport transport(transportConfig);
    static issp::Issp154NetworkManager networkManager(transport,
                                                       kExampleDeviceId);
    static issp::IsspDevice device(deviceConfig, transport);
    static issp::DigitalOutputBehavior behavior(behaviorConfig);
    static issp::Issp154ReportExecutor reportExecutor(device, transport);

    (void)networkManager;
    (void)behavior;
    (void)reportExecutor;
}
