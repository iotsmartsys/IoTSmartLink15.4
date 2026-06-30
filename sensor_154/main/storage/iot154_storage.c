#include "iot154_storage.h"

#include "esp_check.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "iot154_packet.h"
#include "iot154_sensor_config.h"

void iot154_storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
        return;
    }
    ESP_ERROR_CHECK(err);
}

bool iot154_storage_load_central_ext_addr(uint8_t *addr)
{
    nvs_handle_t nvs = 0;
    size_t len = IOT154_EXT_ADDR_LEN;
    esp_err_t err = nvs_open(IOT154_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return false;
    }

    err = nvs_get_blob(nvs, IOT154_NVS_CENTRAL_KEY, addr, &len);
    nvs_close(nvs);
    return err == ESP_OK && len == IOT154_EXT_ADDR_LEN;
}

void iot154_storage_save_central_ext_addr(const uint8_t *addr)
{
    nvs_handle_t nvs = 0;
    ESP_ERROR_CHECK(nvs_open(IOT154_NVS_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_blob(nvs, IOT154_NVS_CENTRAL_KEY, addr, IOT154_EXT_ADDR_LEN));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
}

uint8_t iot154_storage_load_send_failures(void)
{
    nvs_handle_t nvs = 0;
    uint8_t failures = 0;
    esp_err_t err = nvs_open(IOT154_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return 0;
    }

    err = nvs_get_u8(nvs, IOT154_NVS_SEND_FAILURES_KEY, &failures);
    nvs_close(nvs);
    return err == ESP_OK ? failures : 0;
}

void iot154_storage_save_send_failures(uint8_t failures)
{
    nvs_handle_t nvs = 0;
    ESP_ERROR_CHECK(nvs_open(IOT154_NVS_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_u8(nvs, IOT154_NVS_SEND_FAILURES_KEY, failures));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
}

uint8_t iot154_storage_record_send_failure(void)
{
    uint8_t failures = iot154_storage_load_send_failures();
    if (failures < UINT8_MAX) {
        ++failures;
    }
    iot154_storage_save_send_failures(failures);
    return failures;
}

void iot154_storage_reset_send_failures(void)
{
    iot154_storage_save_send_failures(0);
}

void iot154_storage_reset_pairing(void)
{
    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open(IOT154_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return;
    }
    ESP_ERROR_CHECK(err);

    err = nvs_erase_all(nvs);
    if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_ERROR_CHECK(err);
    }
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
}
