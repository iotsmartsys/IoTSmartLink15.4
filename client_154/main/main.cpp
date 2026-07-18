#include <stdint.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "digital_output_behavior.hpp"
#include "iot154_packet.h"
#include "iot154_storage.h"
#include "issp_device.hpp"
#include "issp154_destination_manager.hpp"
#include "issp154_report_executor.hpp"
#include "issp154_transport.hpp"

#define IOT154_RELAY_GPIO GPIO_NUM_13
#define IOT154_RELAY_ENDPOINT_ID 1
#define IOT154_SENSOR_DEVICE_ID 0x15400001

static const char *TAG = "iot154_switch";

static void shutdown_transport_after_failure(issp::Issp154Transport &transport)
{
    const issp::IsspResult result = transport.end();
    if (result != issp::IsspResult::Ok)
    {
        ESP_LOGE(TAG, "ISSP transport shutdown failed: %u",
                 static_cast<unsigned>(result));
    }
}

extern "C" void app_main()
{
    iot154_storage_init();

    static uint8_t switch_ext_addr[IOT154_EXT_ADDR_LEN];
    ESP_ERROR_CHECK(esp_read_mac(switch_ext_addr, ESP_MAC_IEEE802154));

    static const issp::Issp154TransportConfig transport_config = {
        .channel = IOT154_CHANNEL,
        .panId = IOT154_PAN_ID,
        .shortAddress = IOT154_SENSOR_ADDR,
        .coordinator = false,
        .extendedAddress = switch_ext_addr,
        .cca = true,
    };
    static const issp::IsspDeviceConfig device_config = {
        .deviceId = IOT154_SENSOR_DEVICE_ID,
    };
    static const issp::DigitalOutputConfig relay_config = {
        .endpointId = IOT154_RELAY_ENDPOINT_ID,
        .eventType = IOT154_EVENT_POWER,
        .pin = IOT154_RELAY_GPIO,
        .activeLevel = 1,
        .initialState = false,
        .reportOnStart = false,
    };

    static issp::Issp154Transport transport(transport_config);
    static issp::Issp154DestinationManager destination_manager(
        transport,
        IOT154_SENSOR_DEVICE_ID);
    static issp::IsspDevice device(device_config, transport);
    static issp::DigitalOutputBehavior relay_behavior(relay_config);
    static issp::Issp154ReportExecutor report_executor(device, transport);

    const issp::IsspResult transport_result = transport.begin();
    if (transport_result != issp::IsspResult::Ok)
    {
        ESP_LOGE(TAG, "ISSP transport initialization failed: %u",
                 static_cast<unsigned>(transport_result));
        shutdown_transport_after_failure(transport);
        return;
    }

    const issp::IsspResult destination_result =
        destination_manager.initializeDestination();
    if (destination_result != issp::IsspResult::Ok)
    {
        ESP_LOGE(TAG, "ISSP destination initialization failed: %u",
                 static_cast<unsigned>(destination_result));
        shutdown_transport_after_failure(transport);
        return;
    }
    ESP_LOGI(TAG, "ISSP destination initialized");

    const issp::IsspResult add_behavior_result =
        device.addBehavior(relay_behavior);
    if (add_behavior_result != issp::IsspResult::Ok)
    {
        ESP_LOGE(TAG, "ISSP behavior registration failed: %u",
                 static_cast<unsigned>(add_behavior_result));
        shutdown_transport_after_failure(transport);
        return;
    }

    const issp::IsspResult device_result = device.start();
    if (device_result != issp::IsspResult::Ok)
    {
        ESP_LOGE(TAG, "ISSP device initialization failed: %u",
                 static_cast<unsigned>(device_result));
        shutdown_transport_after_failure(transport);
        return;
    }

    const issp::IsspResult executor_result = report_executor.start();
    if (executor_result != issp::IsspResult::Ok)
    {
        ESP_LOGE(TAG, "ISSP report executor initialization failed: %u",
                 static_cast<unsigned>(executor_result));
        shutdown_transport_after_failure(transport);
        return;
    }

    ESP_LOGI(TAG, "ISSP runtime started");
    for (;;)
    {
        vTaskDelay(portMAX_DELAY);
    }
}
