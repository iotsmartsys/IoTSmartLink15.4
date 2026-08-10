#include "device_registry.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#define DEVICE_REGISTRY_TAG "DEVICE_REGISTRY"
#define DEVICE_REGISTRY_ENTRY_WIRE_SIZE (IOT154_EXT_ADDR_LEN + sizeof(uint32_t))
#define DEVICE_REGISTRY_MAX_BLOB_SIZE \
    (2U + (DEVICE_REGISTRY_MAX_ENTRIES * DEVICE_REGISTRY_ENTRY_WIRE_SIZE) + 1U)

static const device_registry_storage_t *s_storage;
static device_registry_entry_t s_entries[DEVICE_REGISTRY_MAX_ENTRIES];
static size_t s_count;
static device_registry_state_t s_state = DEVICE_REGISTRY_STATE_UNAVAILABLE;

static void format_ext_addr(const uint8_t *addr, char *out, size_t out_len)
{
    snprintf(out,
             out_len,
             "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
}

static bool addr_is_valid(const uint8_t *ext_addr)
{
    bool all_zero = true;
    bool all_ff = true;
    for (size_t i = 0; i < IOT154_EXT_ADDR_LEN; ++i)
    {
        if (ext_addr[i] != 0x00)
        {
            all_zero = false;
        }
        if (ext_addr[i] != 0xff)
        {
            all_ff = false;
        }
    }
    return !all_zero && !all_ff;
}

static bool find_index(const uint8_t *ext_addr, size_t *out_index)
{
    for (size_t i = 0; i < s_count; ++i)
    {
        if (iot154_ext_addr_equal(s_entries[i].ext_addr, ext_addr))
        {
            if (out_index != NULL)
            {
                *out_index = i;
            }
            return true;
        }
    }
    return false;
}

static uint8_t checksum(const uint8_t *buffer, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i)
    {
        sum = (uint8_t)(sum + buffer[i]);
    }
    return sum;
}

static size_t serialize(const device_registry_entry_t *entries, size_t count, uint8_t *buffer)
{
    size_t pos = 0;
    buffer[pos++] = DEVICE_REGISTRY_SCHEMA_VERSION;
    buffer[pos++] = (uint8_t)count;
    for (size_t i = 0; i < count; ++i)
    {
        memcpy(&buffer[pos], entries[i].ext_addr, IOT154_EXT_ADDR_LEN);
        pos += IOT154_EXT_ADDR_LEN;
        buffer[pos++] = (uint8_t)(entries[i].device_id & 0xffU);
        buffer[pos++] = (uint8_t)((entries[i].device_id >> 8) & 0xffU);
        buffer[pos++] = (uint8_t)((entries[i].device_id >> 16) & 0xffU);
        buffer[pos++] = (uint8_t)((entries[i].device_id >> 24) & 0xffU);
    }
    buffer[pos] = checksum(buffer, pos);
    pos += 1;
    return pos;
}

/// @brief Validate and decode a persisted blob. Distinguishes "schema not recognized" (caller must
/// start empty) from structural corruption (caller must report the registry unavailable).
typedef enum
{
    PARSE_OK,
    PARSE_SCHEMA_INCOMPATIBLE,
    PARSE_INVALID,
} parse_result_t;

static parse_result_t parse(const uint8_t *buffer,
                            size_t len,
                            device_registry_entry_t *out_entries,
                            size_t *out_count,
                            const char **out_reason)
{
    if (len < 2)
    {
        *out_reason = "truncated";
        return PARSE_INVALID;
    }
    if (buffer[0] != DEVICE_REGISTRY_SCHEMA_VERSION)
    {
        return PARSE_SCHEMA_INCOMPATIBLE;
    }

    const uint8_t count = buffer[1];
    if (count > DEVICE_REGISTRY_MAX_ENTRIES)
    {
        *out_reason = "entry_count_invalid";
        return PARSE_INVALID;
    }

    const size_t expected_len = 2U + ((size_t)count * DEVICE_REGISTRY_ENTRY_WIRE_SIZE) + 1U;
    if (len != expected_len)
    {
        *out_reason = "truncated";
        return PARSE_INVALID;
    }
    if (buffer[len - 1] != checksum(buffer, len - 1))
    {
        *out_reason = "checksum_mismatch";
        return PARSE_INVALID;
    }

    size_t pos = 2;
    for (size_t i = 0; i < count; ++i)
    {
        memcpy(out_entries[i].ext_addr, &buffer[pos], IOT154_EXT_ADDR_LEN);
        pos += IOT154_EXT_ADDR_LEN;
        out_entries[i].device_id = (uint32_t)buffer[pos] |
                                   ((uint32_t)buffer[pos + 1] << 8) |
                                   ((uint32_t)buffer[pos + 2] << 16) |
                                   ((uint32_t)buffer[pos + 3] << 24);
        pos += sizeof(uint32_t);

        if (!addr_is_valid(out_entries[i].ext_addr))
        {
            *out_reason = "invalid_address";
            return PARSE_INVALID;
        }
        for (size_t j = 0; j < i; ++j)
        {
            if (iot154_ext_addr_equal(out_entries[i].ext_addr, out_entries[j].ext_addr))
            {
                *out_reason = "duplicate_address";
                return PARSE_INVALID;
            }
        }
    }

    *out_count = count;
    return PARSE_OK;
}

void device_registry_init(const device_registry_storage_t *storage)
{
    s_storage = storage;
    s_count = 0;
    s_state = DEVICE_REGISTRY_STATE_UNAVAILABLE;
    memset(s_entries, 0, sizeof(s_entries));
}

device_registry_state_t device_registry_load(void)
{
    uint8_t buffer[DEVICE_REGISTRY_MAX_BLOB_SIZE];
    size_t len = sizeof(buffer);
    const esp_err_t err = s_storage->read(s_storage->ctx, buffer, sizeof(buffer), &len);

    if (err == ESP_ERR_NOT_FOUND)
    {
        s_count = 0;
        s_state = DEVICE_REGISTRY_STATE_READY;
        ESP_LOGI(DEVICE_REGISTRY_TAG, "load result=empty entries=0");
        return s_state;
    }
    if (err != ESP_OK)
    {
        s_count = 0;
        s_state = DEVICE_REGISTRY_STATE_UNAVAILABLE;
        ESP_LOGE(DEVICE_REGISTRY_TAG, "load result=unavailable reason=%s", esp_err_to_name(err));
        return s_state;
    }

    device_registry_entry_t parsed[DEVICE_REGISTRY_MAX_ENTRIES];
    size_t parsed_count = 0;
    const char *reason = "unknown";
    const parse_result_t result = parse(buffer, len, parsed, &parsed_count, &reason);

    if (result == PARSE_SCHEMA_INCOMPATIBLE)
    {
        s_count = 0;
        s_state = DEVICE_REGISTRY_STATE_READY;
        ESP_LOGW(DEVICE_REGISTRY_TAG, "load result=incompatible_schema");
        return s_state;
    }
    if (result == PARSE_INVALID)
    {
        s_count = 0;
        s_state = DEVICE_REGISTRY_STATE_UNAVAILABLE;
        ESP_LOGE(DEVICE_REGISTRY_TAG, "load result=unavailable reason=%s", reason);
        return s_state;
    }

    memcpy(s_entries, parsed, sizeof(parsed));
    s_count = parsed_count;
    s_state = DEVICE_REGISTRY_STATE_READY;
    ESP_LOGI(DEVICE_REGISTRY_TAG, "load result=ok entries=%u", (unsigned)s_count);
    return s_state;
}

device_registry_state_t device_registry_state(void)
{
    return s_state;
}

size_t device_registry_count(void)
{
    return s_state == DEVICE_REGISTRY_STATE_READY ? s_count : 0;
}

bool device_registry_find(const uint8_t *ext_addr, uint32_t *out_device_id, size_t *out_index)
{
    if (s_state != DEVICE_REGISTRY_STATE_READY)
    {
        return false;
    }
    size_t index = 0;
    if (!find_index(ext_addr, &index))
    {
        return false;
    }
    if (out_device_id != NULL)
    {
        *out_device_id = s_entries[index].device_id;
    }
    if (out_index != NULL)
    {
        *out_index = index;
    }
    return true;
}

static bool commit(const device_registry_entry_t *entries, size_t count)
{
    uint8_t buffer[DEVICE_REGISTRY_MAX_BLOB_SIZE];
    const size_t len = serialize(entries, count, buffer);
    return s_storage->write(s_storage->ctx, buffer, len) == ESP_OK;
}

device_registry_pair_result_t device_registry_pair(const uint8_t *ext_addr, uint32_t device_id)
{
    char addr_text[3 * IOT154_EXT_ADDR_LEN] = {0};
    format_ext_addr(ext_addr, addr_text, sizeof(addr_text));

    if (s_state != DEVICE_REGISTRY_STATE_READY)
    {
        ESP_LOGW(DEVICE_REGISTRY_TAG, "pairing result=failed reason=registry_unavailable");
        return DEVICE_REGISTRY_PAIR_FAILED;
    }
    if (!addr_is_valid(ext_addr))
    {
        ESP_LOGW(DEVICE_REGISTRY_TAG, "pairing result=failed reason=invalid_address");
        return DEVICE_REGISTRY_PAIR_FAILED;
    }

    size_t index = 0;
    if (find_index(ext_addr, &index))
    {
        if (s_entries[index].device_id == device_id)
        {
            ESP_LOGI(DEVICE_REGISTRY_TAG, "pairing result=known device=0x%08" PRIx32 " addr=%s",
                     device_id, addr_text);
            return DEVICE_REGISTRY_PAIR_KNOWN;
        }

        device_registry_entry_t staged[DEVICE_REGISTRY_MAX_ENTRIES];
        memcpy(staged, s_entries, sizeof(staged));
        staged[index].device_id = device_id;
        if (!commit(staged, s_count))
        {
            ESP_LOGE(DEVICE_REGISTRY_TAG, "pairing result=failed reason=persist");
            return DEVICE_REGISTRY_PAIR_FAILED;
        }
        memcpy(s_entries, staged, sizeof(s_entries));
        ESP_LOGI(DEVICE_REGISTRY_TAG, "pairing result=updated device=0x%08" PRIx32 " addr=%s",
                 device_id, addr_text);
        return DEVICE_REGISTRY_PAIR_UPDATED;
    }

    if (s_count >= DEVICE_REGISTRY_MAX_ENTRIES)
    {
        ESP_LOGW(DEVICE_REGISTRY_TAG, "pairing result=rejected reason=full");
        return DEVICE_REGISTRY_PAIR_REJECTED_FULL;
    }

    device_registry_entry_t staged[DEVICE_REGISTRY_MAX_ENTRIES];
    memcpy(staged, s_entries, sizeof(staged));
    memcpy(staged[s_count].ext_addr, ext_addr, IOT154_EXT_ADDR_LEN);
    staged[s_count].device_id = device_id;
    const size_t new_count = s_count + 1;
    if (!commit(staged, new_count))
    {
        ESP_LOGE(DEVICE_REGISTRY_TAG, "pairing result=failed reason=persist");
        return DEVICE_REGISTRY_PAIR_FAILED;
    }
    memcpy(s_entries, staged, sizeof(s_entries));
    s_count = new_count;
    ESP_LOGI(DEVICE_REGISTRY_TAG, "pairing result=created device=0x%08" PRIx32 " addr=%s",
             device_id, addr_text);
    return DEVICE_REGISTRY_PAIR_CREATED;
}
