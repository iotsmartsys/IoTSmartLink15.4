// Automated coverage for device_registry (ISSP-Coordinator-Paired-Device-Registry.md):
// COORD-REG-AC-002 (atomic failure preserves previous view), AC-003 (capacity without eviction),
// AC-004 (update and idempotency), AC-006 (last_seq never belongs to the blob) and the structural
// load outcomes that AC-001/AC-007 require (absent, incompatible schema, corrupt/unavailable).
// Every test drives device_registry.c through the storage seam with an in-memory fake — nothing
// here touches real NVS, so this app targets esp32c3 and runs under QEMU (idf.py qemu), matching
// the precedent in components/issp_app_154/test_apps/smart_sys_app_test.
//
// Not covered here: AC-007's sentinel/namespace-isolation evidence against real NVS, and AC-001 /
// AC-008's mandatory real-hardware terminal execution. Those remain pending per the specification's
// own gate (section 13): compilation and fakes do not substitute for that evidence.

#include <string.h>

#include "device_registry.h"
#include "unity.h"

#define BLOB_CAPACITY 128

typedef struct
{
    bool present;
    uint8_t buffer[BLOB_CAPACITY];
    size_t length;
    esp_err_t forced_read_result;
    esp_err_t forced_write_result;
    size_t write_calls;
} fake_storage_t;

static fake_storage_t s_fake;

static esp_err_t fake_read(void *ctx, uint8_t *buffer, size_t buffer_size, size_t *out_len)
{
    fake_storage_t *fake = (fake_storage_t *)ctx;
    if (fake->forced_read_result != ESP_OK)
    {
        const esp_err_t result = fake->forced_read_result;
        fake->forced_read_result = ESP_OK;
        return result;
    }
    if (!fake->present)
    {
        return ESP_ERR_NOT_FOUND;
    }
    TEST_ASSERT_TRUE(fake->length <= buffer_size);
    memcpy(buffer, fake->buffer, fake->length);
    *out_len = fake->length;
    return ESP_OK;
}

static esp_err_t fake_write(void *ctx, const uint8_t *buffer, size_t len)
{
    fake_storage_t *fake = (fake_storage_t *)ctx;
    fake->write_calls++;
    if (fake->forced_write_result != ESP_OK)
    {
        const esp_err_t result = fake->forced_write_result;
        fake->forced_write_result = ESP_OK;
        return result;
    }
    TEST_ASSERT_TRUE(len <= BLOB_CAPACITY);
    memcpy(fake->buffer, buffer, len);
    fake->length = len;
    fake->present = true;
    return ESP_OK;
}

static const device_registry_storage_t s_fake_storage = {
    .read = fake_read,
    .write = fake_write,
    .ctx = &s_fake,
};

static void reset_fixture(void)
{
    memset(&s_fake, 0, sizeof(s_fake));
    device_registry_init(&s_fake_storage);
}

/// @brief Reload from whatever the fake currently holds durable, simulating a reboot: a fresh
/// module instance that only sees what commit() actually persisted.
static device_registry_state_t reboot_and_load(void)
{
    device_registry_init(&s_fake_storage);
    return device_registry_load();
}

static const uint8_t kAddrA[IOT154_EXT_ADDR_LEN] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
static const uint8_t kAddrB[IOT154_EXT_ADDR_LEN] = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18};

TEST_CASE("load with no persisted blob starts empty and ready", "[device_registry]")
{
    reset_fixture();
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());
    TEST_ASSERT_EQUAL(0, device_registry_count());
}

TEST_CASE("load with unrecognized schema version starts empty without touching entries", "[device_registry]")
{
    reset_fixture();
    s_fake.present = true;
    s_fake.buffer[0] = DEVICE_REGISTRY_SCHEMA_VERSION + 1;
    s_fake.buffer[1] = 0xff; // would be an invalid count if interpreted; must not be interpreted
    s_fake.length = 2;

    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());
    TEST_ASSERT_EQUAL(0, device_registry_count());
}

TEST_CASE("load with truncated blob is unavailable", "[device_registry]")
{
    reset_fixture();
    s_fake.present = true;
    s_fake.buffer[0] = DEVICE_REGISTRY_SCHEMA_VERSION;
    s_fake.buffer[1] = 1; // claims one entry but the blob is far shorter
    s_fake.length = 2;

    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_UNAVAILABLE, device_registry_load());
    TEST_ASSERT_EQUAL(0, device_registry_count());
}

TEST_CASE("load with bad checksum is unavailable", "[device_registry]")
{
    reset_fixture();
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(kAddrA, 0x1234));

    s_fake.buffer[s_fake.length - 1] ^= 0xff; // corrupt the trailing checksum byte

    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_UNAVAILABLE, reboot_and_load());
    TEST_ASSERT_EQUAL(0, device_registry_count());
}

TEST_CASE("load with a real read error is unavailable", "[device_registry]")
{
    reset_fixture();
    s_fake.forced_read_result = ESP_ERR_INVALID_STATE;
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_UNAVAILABLE, device_registry_load());
}

TEST_CASE("pairing while unavailable fails without writing", "[device_registry]")
{
    reset_fixture();
    s_fake.forced_read_result = ESP_ERR_INVALID_STATE;
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_UNAVAILABLE, device_registry_load());

    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_FAILED, device_registry_pair(kAddrA, 0x1234));
    TEST_ASSERT_EQUAL(0, s_fake.write_calls);
}

// COORD-REG-AC-002 — failed commit preserves the previous durable view and RAM view.
TEST_CASE("write failure preserves the previous entry across reboot", "[device_registry][AC-002]")
{
    reset_fixture();
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(kAddrA, 0x1234));

    s_fake.forced_write_result = ESP_ERR_INVALID_STATE;
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_FAILED, device_registry_pair(kAddrB, 0x5678));

    uint32_t device_id = 0;
    TEST_ASSERT_TRUE(device_registry_find(kAddrA, &device_id, NULL));
    TEST_ASSERT_EQUAL_HEX32(0x1234, device_id);
    TEST_ASSERT_FALSE(device_registry_find(kAddrB, NULL, NULL));
    TEST_ASSERT_EQUAL(1, device_registry_count());

    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, reboot_and_load());
    TEST_ASSERT_EQUAL(1, device_registry_count());
    TEST_ASSERT_TRUE(device_registry_find(kAddrA, &device_id, NULL));
    TEST_ASSERT_EQUAL_HEX32(0x1234, device_id);
    TEST_ASSERT_FALSE(device_registry_find(kAddrB, NULL, NULL));
}

// COORD-REG-AC-003 — full registry rejects a new address without evicting any existing entry.
TEST_CASE("capacity full rejects a new address without evicting existing entries", "[device_registry][AC-003]")
{
    reset_fixture();
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());

    uint8_t addr[IOT154_EXT_ADDR_LEN];
    for (unsigned i = 0; i < DEVICE_REGISTRY_MAX_ENTRIES; ++i)
    {
        memset(addr, 0, sizeof(addr));
        addr[7] = (uint8_t)(i + 1);
        TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(addr, 0x1000 + i));
    }
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_MAX_ENTRIES, device_registry_count());

    memset(addr, 0, sizeof(addr));
    addr[7] = 0x09;
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_REJECTED_FULL, device_registry_pair(addr, 0x9999));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_MAX_ENTRIES, device_registry_count());
    TEST_ASSERT_FALSE(device_registry_find(addr, NULL, NULL));

    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, reboot_and_load());
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_MAX_ENTRIES, device_registry_count());
    for (unsigned i = 0; i < DEVICE_REGISTRY_MAX_ENTRIES; ++i)
    {
        memset(addr, 0, sizeof(addr));
        addr[7] = (uint8_t)(i + 1);
        uint32_t device_id = 0;
        TEST_ASSERT_TRUE(device_registry_find(addr, &device_id, NULL));
        TEST_ASSERT_EQUAL_HEX32(0x1000 + i, device_id);
    }
}

// COORD-REG-AC-004 — repeating the same pair is a no-op write; a changed device_id commits once.
TEST_CASE("repeated identical pairing does not write, changed device_id commits once", "[device_registry][AC-004]")
{
    reset_fixture();
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(kAddrA, 0x1234));
    const size_t writes_after_create = s_fake.write_calls;

    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_KNOWN, device_registry_pair(kAddrA, 0x1234));
    TEST_ASSERT_EQUAL(writes_after_create, s_fake.write_calls);

    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_UPDATED, device_registry_pair(kAddrA, 0x5678));
    TEST_ASSERT_EQUAL(writes_after_create + 1, s_fake.write_calls);
    TEST_ASSERT_EQUAL(1, device_registry_count());

    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, reboot_and_load());
    uint32_t device_id = 0;
    TEST_ASSERT_TRUE(device_registry_find(kAddrA, &device_id, NULL));
    TEST_ASSERT_EQUAL_HEX32(0x5678, device_id);
    TEST_ASSERT_EQUAL(1, device_registry_count());
}

// COORD-REG-AC-006 — last_seq has no place in the persisted schema: the wire format is exactly
// schema_version + entry_count + entries(addr+device_id) + checksum, nothing volatile included.
TEST_CASE("persisted blob size matches schema exactly, with no room for last_seq", "[device_registry][AC-006]")
{
    reset_fixture();
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(kAddrA, 0x1234));

    const size_t expected_len = 2 /* schema_version + entry_count */
                               + 1 * (IOT154_EXT_ADDR_LEN + sizeof(uint32_t))
                               + 1 /* checksum */;
    TEST_ASSERT_EQUAL(expected_len, s_fake.length);
}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
