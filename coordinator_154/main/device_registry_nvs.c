#include "device_registry.h"

#include "nvs.h"

#define DEVICE_REGISTRY_NVS_NAMESPACE "coord_reg"
#define DEVICE_REGISTRY_NVS_KEY "devices"

#ifndef DEVICE_REGISTRY_NVS_TESTING
typedef struct
{
    esp_err_t (*open)(const char *namespace_name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle);
    esp_err_t (*get_blob)(nvs_handle_t handle, const char *key, void *out_value, size_t *length);
    esp_err_t (*set_blob)(nvs_handle_t handle, const char *key, const void *value, size_t length);
    esp_err_t (*commit)(nvs_handle_t handle);
    void (*close)(nvs_handle_t handle);
} device_registry_nvs_ops_t;
#endif

static const device_registry_nvs_ops_t s_production_ops = {
    .open = nvs_open,
    .get_blob = nvs_get_blob,
    .set_blob = nvs_set_blob,
    .commit = nvs_commit,
    .close = nvs_close,
};

static const device_registry_nvs_ops_t *s_ops = &s_production_ops;

static esp_err_t nvs_storage_read(void *ctx, uint8_t *buffer, size_t buffer_size, size_t *out_len)
{
    (void)ctx;
    nvs_handle_t handle = 0;
    esp_err_t err = s_ops->open(DEVICE_REGISTRY_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        return ESP_ERR_NOT_FOUND;
    }
    if (err != ESP_OK)
    {
        return err;
    }

    size_t length = buffer_size;
    err = s_ops->get_blob(handle, DEVICE_REGISTRY_NVS_KEY, buffer, &length);
    s_ops->close(handle);
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
    esp_err_t err = s_ops->open(DEVICE_REGISTRY_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = s_ops->set_blob(handle, DEVICE_REGISTRY_NVS_KEY, buffer, len);
    if (err == ESP_OK)
    {
        err = s_ops->commit(handle);
    }
    s_ops->close(handle);
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

#ifdef DEVICE_REGISTRY_NVS_TESTING
void device_registry_nvs_set_ops_for_test(const device_registry_nvs_ops_t *ops)
{
    s_ops = ops != NULL ? ops : &s_production_ops;
}

void device_registry_nvs_reset_ops_for_test(void)
{
    s_ops = &s_production_ops;
}
#endif
