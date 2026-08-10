// Automated coverage for device_registry (ISSP-Coordinator-Paired-Device-Registry.md):
// COORD-REG-AC-002 (atomic failure preserves previous view, including the G3-F path through
// device_registry_nvs.c under a commit-boundary failure), AC-003 (capacity without eviction),
// AC-004 (update and idempotency), AC-006 (last_seq never belongs to the blob, schema-size half)
// and the structural load outcomes that AC-001/AC-007 require: absent, incompatible schema,
// truncated, entry_count above capacity, null/broadcast/duplicate address and checksum coverage.
// Tests drive device_registry.c/device_registry_nvs.c through storage seams and exercise the same
// device_registry_policy.c decisions used by main.c for G1 policy coverage. The
// app targets a physical esp32c6, the target bound to the coordinator and its registry
// (TESTEXEC-008), and its runner captures the terminal Unity result over serial,
// per docs/specs/Repository-Test-Execution-Policy.md (physical runner only).
//
// Every TEST_CASE tag here is a "-partial-..." subset per the manifest rule in section 13: this
// app exercises G1 policy decisions, G2 and part of G3-F, but never real NVS on physical hardware
// (G3-N) nor
// the end-to-end radio/hardware flow (G5). No test here may claim a bare "[AC-00N]" label. Still
// pending, and not claimed as complete evidence by this app: the NVS-initialization-error classes of AC-007
// (ESP_ERR_NVS_NO_FREE_PAGES / ESP_ERR_NVS_NEW_VERSION_FOUND, which are decided in main.c, not
// here); AC-007's sentinel/namespace-isolation evidence against real NVS (G3-N); and AC-001/AC-008's
// mandatory real-hardware terminal execution (G3-N/G5). Compilation and fakes do not substitute
// for that evidence (section 13).

#include <string.h>

#include "device_registry.h"
#include "device_registry_policy.h"
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

typedef struct
{
    bool durable_present;
    uint8_t durable[BLOB_CAPACITY];
    size_t durable_length;
    bool staging_present;
    uint8_t staging[BLOB_CAPACITY];
    size_t staging_length;
    uint8_t sentinel[16];
    esp_err_t forced_set_result;
    esp_err_t forced_commit_result;
    size_t set_calls;
    size_t commit_calls;
} faithful_storage_t;

static faithful_storage_t s_faithful;

static esp_err_t faithful_read(void *ctx, uint8_t *buffer, size_t buffer_size, size_t *out_len)
{
    faithful_storage_t *fake = (faithful_storage_t *)ctx;
    if (!fake->durable_present)
    {
        return ESP_ERR_NOT_FOUND;
    }
    TEST_ASSERT_TRUE(fake->durable_length <= buffer_size);
    memcpy(buffer, fake->durable, fake->durable_length);
    *out_len = fake->durable_length;
    return ESP_OK;
}

static esp_err_t faithful_write(void *ctx, const uint8_t *buffer, size_t len)
{
    faithful_storage_t *fake = (faithful_storage_t *)ctx;
    ++fake->set_calls;
    if (fake->forced_set_result != ESP_OK)
    {
        const esp_err_t result = fake->forced_set_result;
        fake->forced_set_result = ESP_OK;
        return result;
    }

    memcpy(fake->staging, buffer, len);
    fake->staging_length = len;
    fake->staging_present = true;
    ++fake->commit_calls;
    if (fake->forced_commit_result != ESP_OK)
    {
        const esp_err_t result = fake->forced_commit_result;
        fake->forced_commit_result = ESP_OK;
        fake->staging_present = false;
        return result;
    }

    memcpy(fake->durable, fake->staging, fake->staging_length);
    fake->durable_length = fake->staging_length;
    fake->durable_present = true;
    fake->staging_present = false;
    return ESP_OK;
}

static const device_registry_storage_t s_faithful_storage = {
    .read = faithful_read,
    .write = faithful_write,
    .ctx = &s_faithful,
};

static void reset_faithful_fixture(void)
{
    memset(&s_faithful, 0, sizeof(s_faithful));
    memset(s_faithful.sentinel, 0xa5, sizeof(s_faithful.sentinel));
    device_registry_init(&s_faithful_storage);
}

static device_registry_state_t reboot_faithful_and_load(void)
{
    s_faithful.staging_present = false;
    device_registry_init(&s_faithful_storage);
    return device_registry_load();
}

typedef struct
{
    bool durable_present;
    uint8_t durable[BLOB_CAPACITY];
    size_t durable_length;
    bool staging_present;
    uint8_t staging[BLOB_CAPACITY];
    size_t staging_length;
    uint8_t sentinel[16];
    esp_err_t forced_commit_result;
    size_t set_calls;
    size_t commit_calls;
} nvs_ops_fake_t;

static nvs_ops_fake_t s_nvs_fake;

static esp_err_t nvs_fake_open(const char *namespace_name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle)
{
    TEST_ASSERT_EQUAL_STRING("coord_reg", namespace_name);
    TEST_ASSERT_TRUE(open_mode == NVS_READONLY || open_mode == NVS_READWRITE);
    *out_handle = 1;
    return ESP_OK;
}

static esp_err_t nvs_fake_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length)
{
    TEST_ASSERT_EQUAL(1, handle);
    TEST_ASSERT_EQUAL_STRING("devices", key);
    if (!s_nvs_fake.durable_present)
    {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    TEST_ASSERT_TRUE(s_nvs_fake.durable_length <= *length);
    memcpy(out_value, s_nvs_fake.durable, s_nvs_fake.durable_length);
    *length = s_nvs_fake.durable_length;
    return ESP_OK;
}

static esp_err_t nvs_fake_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length)
{
    TEST_ASSERT_EQUAL(1, handle);
    TEST_ASSERT_EQUAL_STRING("devices", key);
    ++s_nvs_fake.set_calls;
    memcpy(s_nvs_fake.staging, value, length);
    s_nvs_fake.staging_length = length;
    s_nvs_fake.staging_present = true;
    return ESP_OK;
}

static esp_err_t nvs_fake_commit(nvs_handle_t handle)
{
    TEST_ASSERT_EQUAL(1, handle);
    ++s_nvs_fake.commit_calls;
    if (s_nvs_fake.forced_commit_result != ESP_OK)
    {
        const esp_err_t result = s_nvs_fake.forced_commit_result;
        s_nvs_fake.forced_commit_result = ESP_OK;
        return result;
    }
    memcpy(s_nvs_fake.durable, s_nvs_fake.staging, s_nvs_fake.staging_length);
    s_nvs_fake.durable_length = s_nvs_fake.staging_length;
    s_nvs_fake.durable_present = true;
    return ESP_OK;
}

static void nvs_fake_close(nvs_handle_t handle)
{
    TEST_ASSERT_EQUAL(1, handle);
    s_nvs_fake.staging_present = false;
}

static const device_registry_nvs_ops_t s_nvs_fake_ops = {
    .open = nvs_fake_open,
    .get_blob = nvs_fake_get_blob,
    .set_blob = nvs_fake_set_blob,
    .commit = nvs_fake_commit,
    .close = nvs_fake_close,
};

static void reset_nvs_adapter_fixture(void)
{
    memset(&s_nvs_fake, 0, sizeof(s_nvs_fake));
    memset(s_nvs_fake.sentinel, 0x5a, sizeof(s_nvs_fake.sentinel));
    device_registry_nvs_set_ops_for_test(&s_nvs_fake_ops);
    device_registry_init(device_registry_nvs_storage());
}

static void reset_fixture(void)
{
    memset(&s_fake, 0, sizeof(s_fake));
    device_registry_init(&s_fake_storage);
}

/// module instance that only sees what commit() actually persisted.
static const uint8_t kAddrA[IOT154_EXT_ADDR_LEN] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
static const uint8_t kAddrB[IOT154_EXT_ADDR_LEN] = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18};
static device_registry_state_t reboot_and_load(void)
{
    device_registry_init(&s_fake_storage);
    return device_registry_load();
}

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

TEST_CASE("checksum covers address and device id bytes", "[device_registry][AC-007-partial-schema]")
{
    reset_fixture();
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(kAddrA, 0x1234));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(kAddrB, 0x5678));

    s_fake.buffer[2] ^= 0x80;
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_UNAVAILABLE, reboot_and_load());

    reset_fixture();
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(kAddrA, 0x1234));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(kAddrB, 0x5678));
    s_fake.buffer[2 + IOT154_EXT_ADDR_LEN] ^= 0x80;
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_UNAVAILABLE, reboot_and_load());
}

// COORD-REG-AC-007 class 2 — entry_count above the eight-entry capacity is unavailable, without
// interpreting any byte as an entry (section 10).
TEST_CASE("load with entry count above capacity is unavailable", "[device_registry][AC-007-partial-count]")
{
    reset_fixture();
    s_fake.present = true;
    s_fake.buffer[0] = DEVICE_REGISTRY_SCHEMA_VERSION;
    s_fake.buffer[1] = DEVICE_REGISTRY_MAX_ENTRIES + 1;
    s_fake.length = 2;

    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_UNAVAILABLE, device_registry_load());
    TEST_ASSERT_EQUAL(0, device_registry_count());
}

// COORD-REG-AC-007 class 3 — a structurally valid, correctly checksummed blob whose address is
// null must still be rejected as unavailable (section 5, "endereço não nulo").
TEST_CASE("load with a null extended address is unavailable", "[device_registry][AC-007-partial-address]")
{
    reset_fixture();
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(kAddrA, 0x1234));

    uint8_t original_addr[IOT154_EXT_ADDR_LEN];
    memcpy(original_addr, &s_fake.buffer[2], sizeof(original_addr));
    int16_t delta = 0;
    for (size_t i = 0; i < IOT154_EXT_ADDR_LEN; ++i)
    {
        delta = (int16_t)(delta + 0x00 - (int16_t)original_addr[i]);
    }
    memset(&s_fake.buffer[2], 0x00, IOT154_EXT_ADDR_LEN);
    s_fake.buffer[s_fake.length - 1] = (uint8_t)(s_fake.buffer[s_fake.length - 1] + delta);

    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_UNAVAILABLE, reboot_and_load());
    TEST_ASSERT_EQUAL(0, device_registry_count());
}

// COORD-REG-AC-007 class 3 — same as above for the broadcast address (section 5, "diferente de
// broadcast").
TEST_CASE("load with a broadcast extended address is unavailable", "[device_registry][AC-007-partial-address]")
{
    reset_fixture();
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(kAddrA, 0x1234));

    uint8_t original_addr[IOT154_EXT_ADDR_LEN];
    memcpy(original_addr, &s_fake.buffer[2], sizeof(original_addr));
    int16_t delta = 0;
    for (size_t i = 0; i < IOT154_EXT_ADDR_LEN; ++i)
    {
        delta = (int16_t)(delta + 0xff - (int16_t)original_addr[i]);
    }
    memset(&s_fake.buffer[2], 0xff, IOT154_EXT_ADDR_LEN);
    s_fake.buffer[s_fake.length - 1] = (uint8_t)(s_fake.buffer[s_fake.length - 1] + delta);

    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_UNAVAILABLE, reboot_and_load());
    TEST_ASSERT_EQUAL(0, device_registry_count());
}

// COORD-REG-AC-007 class 4 — two entries sharing the same extended address must be rejected as
// unavailable (section 5, "endereços sem duplicidade"), even though each address is individually
// valid and the checksum is recomputed to stay internally consistent.
TEST_CASE("load with a duplicated extended address is unavailable", "[device_registry][AC-007-partial-duplicate]")
{
    reset_fixture();
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(kAddrA, 0x1234));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(kAddrB, 0x5678));

    const size_t entry_size = IOT154_EXT_ADDR_LEN + sizeof(uint32_t);
    const size_t second_addr_offset = 2 + entry_size;
    uint8_t original_addr[IOT154_EXT_ADDR_LEN];
    memcpy(original_addr, &s_fake.buffer[second_addr_offset], sizeof(original_addr));

    int16_t delta = 0;
    for (size_t i = 0; i < IOT154_EXT_ADDR_LEN; ++i)
    {
        delta = (int16_t)(delta + (int16_t)kAddrA[i] - (int16_t)original_addr[i]);
    }
    memcpy(&s_fake.buffer[second_addr_offset], kAddrA, IOT154_EXT_ADDR_LEN);
    s_fake.buffer[s_fake.length - 1] = (uint8_t)(s_fake.buffer[s_fake.length - 1] + delta);

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
// Partial: the single-buffer fake only fails the write atomically as a whole; it does not model a
// separate staging/commit boundary (see "staging failures..." below for that half of the gate).
TEST_CASE("write failure preserves the previous entry across reboot", "[device_registry][AC-002-partial-core]")
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

TEST_CASE("staging failures preserve durable registry and sentinel", "[device_registry][AC-002-partial-g2]")
{
    uint8_t sentinel_before[sizeof(s_faithful.sentinel)];
    reset_faithful_fixture();
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(kAddrA, 0x1234));
    memcpy(sentinel_before, s_faithful.sentinel, sizeof(sentinel_before));

    s_faithful.forced_set_result = ESP_ERR_INVALID_STATE;
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_FAILED, device_registry_pair(kAddrB, 0x5678));
    TEST_ASSERT_EQUAL(2, s_faithful.set_calls);
    TEST_ASSERT_EQUAL(1, s_faithful.commit_calls);
    TEST_ASSERT_FALSE(s_faithful.staging_present);
    TEST_ASSERT_EQUAL_MEMORY(sentinel_before, s_faithful.sentinel, sizeof(sentinel_before));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, reboot_faithful_and_load());
    TEST_ASSERT_TRUE(device_registry_find(kAddrA, NULL, NULL));
    TEST_ASSERT_FALSE(device_registry_find(kAddrB, NULL, NULL));

    s_faithful.forced_commit_result = ESP_ERR_INVALID_STATE;
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_FAILED, device_registry_pair(kAddrB, 0x5678));
    TEST_ASSERT_EQUAL(3, s_faithful.set_calls);
    TEST_ASSERT_EQUAL(2, s_faithful.commit_calls);
    TEST_ASSERT_FALSE(s_faithful.staging_present);
    TEST_ASSERT_EQUAL_MEMORY(sentinel_before, s_faithful.sentinel, sizeof(sentinel_before));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, reboot_faithful_and_load());
    TEST_ASSERT_TRUE(device_registry_find(kAddrA, NULL, NULL));
    TEST_ASSERT_FALSE(device_registry_find(kAddrB, NULL, NULL));
}

// COORD-REG-AC-003 — full registry rejects a new address without evicting any existing entry.
// Partial: calls device_registry_pair() directly; it does not observe the DISCOVERY_RESP/radio
// effect that the gate's integrated evidence (G1) requires.
TEST_CASE("capacity full rejects a new address without evicting existing entries", "[device_registry][AC-003-partial-core]")
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
// Partial: exercises device_registry.c only; it does not observe a restored command using the
// updated device_id, which the gate's integrated evidence (G1) requires.
TEST_CASE("repeated identical pairing does not write, changed device_id commits once", "[device_registry][AC-004-partial-core]")
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
// Partial: checks blob size only; the gate also requires observing the event/duplicate/reboot/
// resend sequence and decoding the blob to demonstrate last_seq's absence (section 13).
TEST_CASE("persisted blob size matches schema exactly, with no room for last_seq", "[device_registry][AC-006-partial-schema]")
{
    reset_fixture();
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(kAddrA, 0x1234));

    const size_t expected_len = 2 /* schema_version + entry_count */
                               + 1 * (IOT154_EXT_ADDR_LEN + sizeof(uint32_t))
                               + 1 /* checksum */;
    TEST_ASSERT_EQUAL(expected_len, s_fake.length);
}

TEST_CASE("production adapter propagates post-staging commit failure", "[device_registry][AC-002-partial-g3f]")
{
    uint8_t sentinel_before[sizeof(s_nvs_fake.sentinel)];
    reset_nvs_adapter_fixture();
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_CREATED, device_registry_pair(kAddrA, 0x1234));
    memcpy(sentinel_before, s_nvs_fake.sentinel, sizeof(sentinel_before));

    s_nvs_fake.forced_commit_result = ESP_ERR_INVALID_STATE;
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_PAIR_FAILED, device_registry_pair(kAddrB, 0x5678));
    TEST_ASSERT_EQUAL(2, s_nvs_fake.set_calls);
    TEST_ASSERT_EQUAL(2, s_nvs_fake.commit_calls);
    TEST_ASSERT_FALSE(s_nvs_fake.staging_present);
    TEST_ASSERT_EQUAL_MEMORY(sentinel_before, s_nvs_fake.sentinel, sizeof(sentinel_before));
    TEST_ASSERT_TRUE(device_registry_find(kAddrA, NULL, NULL));
    TEST_ASSERT_FALSE(device_registry_find(kAddrB, NULL, NULL));

    device_registry_init(device_registry_nvs_storage());
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_STATE_READY, device_registry_load());
    TEST_ASSERT_TRUE(device_registry_find(kAddrA, NULL, NULL));
    TEST_ASSERT_FALSE(device_registry_find(kAddrB, NULL, NULL));
    device_registry_nvs_reset_ops_for_test();
}

TEST_CASE("integrated discovery policy preserves registry and window precedence", "[device_registry][AC-001-partial-g1][AC-007-partial-g1]")
{
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_DISCOVERY_REJECT_UNAVAILABLE,
                      device_registry_policy_discovery(DEVICE_REGISTRY_STATE_UNAVAILABLE, true));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_DISCOVERY_REJECT_UNAVAILABLE,
                      device_registry_policy_discovery(DEVICE_REGISTRY_STATE_UNAVAILABLE, false));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_DISCOVERY_REJECT_WINDOW_CLOSED,
                      device_registry_policy_discovery(DEVICE_REGISTRY_STATE_READY, false));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_DISCOVERY_PROCESS,
                      device_registry_policy_discovery(DEVICE_REGISTRY_STATE_READY, true));
}

TEST_CASE("integrated discovery response follows durable pairing outcome", "[device_registry][AC-001-partial-g1][AC-002-partial-g1][AC-003-partial-g1]")
{
    TEST_ASSERT_TRUE(device_registry_policy_discovery_response(DEVICE_REGISTRY_PAIR_KNOWN));
    TEST_ASSERT_TRUE(device_registry_policy_discovery_response(DEVICE_REGISTRY_PAIR_UPDATED));
    TEST_ASSERT_TRUE(device_registry_policy_discovery_response(DEVICE_REGISTRY_PAIR_CREATED));
    TEST_ASSERT_FALSE(device_registry_policy_discovery_response(DEVICE_REGISTRY_PAIR_REJECTED_FULL));
    TEST_ASSERT_FALSE(device_registry_policy_discovery_response(DEVICE_REGISTRY_PAIR_FAILED));
}

TEST_CASE("integrated data policy fails closed when registry is unavailable", "[device_registry][AC-007-partial-g1]")
{
    const device_registry_data_effects_t effects = device_registry_policy_data(
        DEVICE_REGISTRY_STATE_UNAVAILABLE, true, true, false);
    TEST_ASSERT_FALSE(effects.emit_host_event);
    TEST_ASSERT_FALSE(effects.emit_ack);
    TEST_ASSERT_FALSE(effects.log_unknown_device);
    TEST_ASSERT_TRUE(effects.log_registry_unavailable);
}

TEST_CASE("integrated data policy keeps an unknown origin unpaired", "[device_registry][AC-005-partial-g1]")
{
    device_registry_data_effects_t effects = device_registry_policy_data(
        DEVICE_REGISTRY_STATE_READY, false, false, false);
    TEST_ASSERT_FALSE(effects.emit_host_event);
    TEST_ASSERT_FALSE(effects.emit_ack);
    TEST_ASSERT_TRUE(effects.log_unknown_device);

    effects = device_registry_policy_data(DEVICE_REGISTRY_STATE_READY, true, false, false);
    TEST_ASSERT_TRUE(effects.emit_host_event);
    TEST_ASSERT_TRUE(effects.emit_ack);
    TEST_ASSERT_FALSE(effects.log_unknown_device);
    TEST_ASSERT_FALSE(effects.log_registry_unavailable);
}

TEST_CASE("integrated data policy preserves volatile duplicate effects", "[device_registry][AC-006-partial-g1]")
{
    device_registry_data_effects_t effects = device_registry_policy_data(
        DEVICE_REGISTRY_STATE_READY, false, true, false);
    TEST_ASSERT_TRUE(effects.emit_host_event);
    TEST_ASSERT_TRUE(effects.emit_ack);

    effects = device_registry_policy_data(DEVICE_REGISTRY_STATE_READY, false, true, true);
    TEST_ASSERT_FALSE(effects.emit_host_event);
    TEST_ASSERT_TRUE(effects.emit_ack);
}

TEST_CASE("integrated ack policy requires ready registry identity and correlation", "[device_registry][AC-007-partial-g1][AC-008-partial-g1]")
{
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_ACK_REJECT_UNAVAILABLE,
                      device_registry_policy_ack(DEVICE_REGISTRY_STATE_UNAVAILABLE, true, true));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_ACK_IGNORE,
                      device_registry_policy_ack(DEVICE_REGISTRY_STATE_READY, false, true));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_ACK_IGNORE,
                      device_registry_policy_ack(DEVICE_REGISTRY_STATE_READY, true, false));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_ACK_COMPLETE,
                      device_registry_policy_ack(DEVICE_REGISTRY_STATE_READY, true, true));
}

TEST_CASE("host command policy applies validity registry pending and identity precedence", "[device_registry][AC-005-partial-g1][AC-007-partial-g1]")
{
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_HOST_COMMAND_INVALID_ADDRESS,
                      device_registry_policy_host_command(false, DEVICE_REGISTRY_STATE_UNAVAILABLE, true, false));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_HOST_COMMAND_REGISTRY_UNAVAILABLE,
                      device_registry_policy_host_command(true, DEVICE_REGISTRY_STATE_UNAVAILABLE, true, false));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_HOST_COMMAND_PENDING,
                      device_registry_policy_host_command(true, DEVICE_REGISTRY_STATE_READY, true, true));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_HOST_COMMAND_UNKNOWN_DEVICE,
                      device_registry_policy_host_command(true, DEVICE_REGISTRY_STATE_READY, false, false));
    TEST_ASSERT_EQUAL(DEVICE_REGISTRY_HOST_COMMAND_START,
                      device_registry_policy_host_command(true, DEVICE_REGISTRY_STATE_READY, false, true));
}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
