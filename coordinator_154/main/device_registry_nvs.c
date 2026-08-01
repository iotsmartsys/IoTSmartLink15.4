#include "device_registry.h"

#include "nvs.h"

#define DEVICE_REGISTRY_NVS_NAMESPACE "coord_reg"
#define DEVICE_REGISTRY_NVS_KEY "devices"

static esp_err_t nvs_storage_read(void *ctx, uint8_t *buffer, size_t buffer_size, size_t *out_len)
{
    (void)ctx;
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(DEVICE_REGISTRY_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        return ESP_ERR_NOT_FOUND;
    }
    if (err != ESP_OK)
    {
        return err;
    }

    size_t length = buffer_size;
    err = nvs_get_blob(handle, DEVICE_REGISTRY_NVS_KEY, buffer, &length);
    nvs_close(handle);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        return ESP_ERR_NOT_FOUND;
    }
    if (err != ESP_OK)
    {
        return err;
    }

    *out_len = length;
    return ESP_OK;
}

/// @brief Replace the blob completely: set + commit, so a mid-write failure leaves the previous
/// commit durable and never exposes a partially new registry after reboot.
static esp_err_t nvs_storage_write(void *ctx, const uint8_t *buffer, size_t len)
{
    (void)ctx;
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(DEVICE_REGISTRY_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_blob(handle, DEVICE_REGISTRY_NVS_KEY, buffer, len);
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static const device_registry_storage_t s_nvs_storage = {
    .read = nvs_storage_read,
    .write = nvs_storage_write,
    .ctx = NULL,
};

const device_registry_storage_t *device_registry_nvs_storage(void)
{
    return &s_nvs_storage;
}
