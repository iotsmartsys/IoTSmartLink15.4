#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "iot154_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEVICE_REGISTRY_MAX_ENTRIES 8
#define DEVICE_REGISTRY_SCHEMA_VERSION 1

typedef struct
{
    uint8_t ext_addr[IOT154_EXT_ADDR_LEN];
    uint32_t device_id;
} device_registry_entry_t;

typedef enum
{
    DEVICE_REGISTRY_STATE_READY = 0,
    DEVICE_REGISTRY_STATE_UNAVAILABLE = 1,
} device_registry_state_t;

typedef enum
{
    DEVICE_REGISTRY_PAIR_KNOWN = 0,
    DEVICE_REGISTRY_PAIR_UPDATED,
    DEVICE_REGISTRY_PAIR_CREATED,
    DEVICE_REGISTRY_PAIR_REJECTED_FULL,
    DEVICE_REGISTRY_PAIR_FAILED,
} device_registry_pair_result_t;

/// @brief Storage seam that isolates NVS access from commissioning/pairing logic.
///
/// Implementations must preserve read/atomic-write/failure semantics: `read`
/// reports ESP_ERR_NOT_FOUND when the blob is absent, ESP_OK with the blob
/// copied into `buffer`/`out_len` on success, and any other esp_err_t for a
/// real read error. `write` must either make the full new blob durable and
/// return ESP_OK, or leave the previously durable content unchanged and
/// return a non-ESP_OK error; it must never leave a partially written blob
/// observable.
typedef struct
{
    esp_err_t (*read)(void *ctx, uint8_t *buffer, size_t buffer_size, size_t *out_len);
    esp_err_t (*write)(void *ctx, const uint8_t *buffer, size_t len);
    void *ctx;
} device_registry_storage_t;

/// @brief Production storage backed by NVS, in a namespace private to the coordinator registry.
const device_registry_storage_t *device_registry_nvs_storage(void);

#ifdef DEVICE_REGISTRY_NVS_TESTING
#include "nvs.h"

typedef struct
{
    esp_err_t (*open)(const char *namespace_name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle);
    esp_err_t (*get_blob)(nvs_handle_t handle, const char *key, void *out_value, size_t *length);
    esp_err_t (*set_blob)(nvs_handle_t handle, const char *key, const void *value, size_t length);
    esp_err_t (*commit)(nvs_handle_t handle);
    void (*close)(nvs_handle_t handle);
} device_registry_nvs_ops_t;

/// @brief Replace NVS primitives in test builds while retaining the production adapter control flow.
void device_registry_nvs_set_ops_for_test(const device_registry_nvs_ops_t *ops);
void device_registry_nvs_reset_ops_for_test(void);
#endif

/// @brief Bind the storage implementation and reset in-RAM state. Call once before device_registry_load().
void device_registry_init(const device_registry_storage_t *storage);

/// @brief Load and validate the persisted registry. Must run before device traffic is accepted.
device_registry_state_t device_registry_load(void);

/// @brief Current registry state as of the last load() or pair() outcome.
device_registry_state_t device_registry_state(void);

/// @brief Number of entries currently held in RAM (0 when state is Unavailable).
size_t device_registry_count(void);

/// @brief Look up a paired device by extended address.
/// @param out_device_id optional; receives the persisted device_id on match.
/// @param out_index optional; receives the stable RAM slot index on match, for volatile per-device state.
bool device_registry_find(const uint8_t *ext_addr, uint32_t *out_device_id, size_t *out_index);

/// @brief Execute the pairing transaction of a DISCOVERY_REQ (create, update, idempotent no-op, or reject).
device_registry_pair_result_t device_registry_pair(const uint8_t *ext_addr, uint32_t device_id);

#ifdef __cplusplus
}
#endif
