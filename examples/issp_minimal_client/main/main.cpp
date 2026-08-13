#include <cstdint>

#include "SmartSysApp.h"
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

// A consumer supplies its own source of report identities. The example is a
// compile/link integration proof and never publishes, so a deterministic
// counter is enough and keeps the example free of platform randomness.
std::uint64_t exampleReportId(void *context)
{
    (void)context;
    static std::uint64_t next = 0;
    ++next;
    return next;
}
}

// Constructing and configuring the facade proves compile and link
// integration for issp_app_154. setup() is deliberately not called, so this
// has no radio or NVS side effects.
static iotsmartsys::SmartSysApp facadeApp({
    .deviceId = kExampleDeviceId,
});

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
        .reportIdGenerator = &exampleReportId,
        .reportIdGeneratorContext = nullptr,
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

    facadeApp.addSwitchPlugCapability({
        .pin = GPIO_NUM_NC,
        .activeHigh = true,
        .initialState = false,
        .reportOnStart = false,
        .endpointId = kExampleEndpointId,
        .eventType = kExampleEventType,
    });
    facadeApp.configureFactoryResetButton({
        .pin = GPIO_NUM_NC,
        .activeLow = true,
        .holdTimeMs = 10000,
        .pollIntervalMs = 20,
    });
}
