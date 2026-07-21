#include <stdint.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "digital_output_behavior.hpp"
#include "iot154_packet.h"
#include "issp_device.hpp"
#include "issp154_network_manager.hpp"
#include "issp154_report_executor.hpp"
#include "issp154_transport.hpp"
#include "factory_reset_service.hpp"
#include "reset_button_monitor.hpp"

#define IOT154_RELAY_GPIO GPIO_NUM_13
#define IOT154_RESET_BUTTON_GPIO GPIO_NUM_9
#define IOT154_RELAY_ENDPOINT_ID 1
#define IOT154_SENSOR_DEVICE_ID 0x15400001

static const char *TAG = "iot154_switch";

static void initialize_nvs()
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
        result == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
}

static esp_err_t clear_network_configuration(void *context)
{
    if (context == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }
    const issp::IsspResult result =
        static_cast<issp::Issp154NetworkManager *>(context)
            ->clearPersistedNetwork();
    return result == issp::IsspResult::Ok ? ESP_OK : ESP_FAIL;
}

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
    initialize_nvs();

    static uint8_t switch_ext_addr[IOT154_EXT_ADDR_LEN];
    ESP_ERROR_CHECK(esp_read_mac(switch_ext_addr, ESP_MAC_IEEE802154));

    static const issp::Issp154TransportConfig transport_config = {
        .channel = 0,
        .panId = 0,
        .shortAddress = IOT154_SENSOR_ADDR,
        .coordinator = false,
        .extendedAddress = switch_ext_addr,
        .cca = true,
        .promiscuous = false,
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
        .reportOnStart = true,
    };

    static issp::Issp154Transport transport(transport_config);
    static issp::Issp154NetworkManager network_manager(
        transport,
        IOT154_SENSOR_DEVICE_ID);
    static issp::IsspDevice device(device_config, transport);
    static issp::DigitalOutputBehavior relay_behavior(relay_config);
    static issp::Issp154ReportExecutor report_executor(device, transport);
    static FactoryResetService factory_reset_service(
        clear_network_configuration,
        &network_manager);
    static const ResetButtonConfig reset_button_config = {
        .gpio = IOT154_RESET_BUTTON_GPIO,
        .holdTimeMs = 10000,
        .pollIntervalMs = 20,
        .activeLow = true,
    };
    static ResetButtonMonitor reset_button_monitor(
        reset_button_config,
        factory_reset_service);

    const esp_err_t reset_button_result = reset_button_monitor.start();
    if (reset_button_result != ESP_OK)
    {
        ESP_LOGE(TAG, "Factory reset button initialization failed: %s",
                 esp_err_to_name(reset_button_result));
        return;
    }

    const issp::IsspResult network_result = network_manager.initializeNetwork();
    if (network_result != issp::IsspResult::Ok)
    {
        ESP_LOGE(TAG, "ISSP network initialization failed: %u",
                 static_cast<unsigned>(network_result));
        shutdown_transport_after_failure(transport);
        return;
    }
    ESP_LOGI(TAG, "ISSP network initialized");

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
