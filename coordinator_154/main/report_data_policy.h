#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "device_registry_policy.h"

/// Volatile deduplication window and the DATA decision that governs it.
///
/// This module is private to coordinator_154. It concentrates the decision of
/// new report, retry, identity conflict and unavailable local acceptance so the
/// same code governs production and tests; main.c keeps the radio and the
/// effects. It is deliberately not a shared component, not a public header and
/// not a second policy: the registry policy remains an input of the decision.

/// Section 8.1: fixed capacity, per registry slot, independent of the sequence.
/// It matches the eight pending report slots of a client, which bounds the
/// identities that its normal flow can still re-present.
#define REPORT_DEDUP_WINDOW_CAPACITY 8

/// Section 5: the fingerprint of a logical report for a known device. The
/// sequence deliberately does not take part in it.
typedef struct
{
    uint64_t report_id;
    uint8_t endpoint_id;
    uint8_t event_type;
    uint8_t value;
} report_fingerprint_t;

/// One received DATA frame, already validated for frame, addressing, version,
/// checksum and type/report_id combination.
typedef struct
{
    uint32_t device_id;
    uint64_t report_id;
    uint16_t seq;
    uint8_t endpoint_id;
    uint8_t event_type;
    uint8_t value;
} report_data_input_t;

typedef enum
{
    /// Registry unavailable: no event, no ACK, no window mutation.
    REPORT_DATA_OUTCOME_REGISTRY_UNAVAILABLE = 0,
    /// Unknown origin with the join window closed: rejected as today.
    REPORT_DATA_OUTCOME_UNKNOWN_IGNORED,
    /// Unknown origin accepted by the window policy in force. Forwarded and
    /// ACKed without any deduplication promise between retries.
    REPORT_DATA_OUTCOME_UNKNOWN_FORWARDED,
    /// Unknown origin whose event could not be accepted locally: no ACK.
    REPORT_DATA_OUTCOME_UNKNOWN_LOCAL_UNAVAILABLE,
    /// New identity: event accepted locally, fingerprint cached, ACK attempted.
    REPORT_DATA_OUTCOME_NEW_ACCEPTED,
    /// New identity whose event could not be accepted locally. The identity is
    /// not cached and no ACK is sent, so the client may repeat the report.
    REPORT_DATA_OUTCOME_NEW_LOCAL_UNAVAILABLE,
    /// Known identity with the same content: no second event, ACK attempted.
    REPORT_DATA_OUTCOME_RETRY_DEDUPLICATED,
    /// Known identity with different content: no event, no window change, no ACK.
    REPORT_DATA_OUTCOME_CONFLICT,
} report_data_outcome_t;

/// Substitutable effects. `emit_event` must report the complete local
/// acceptance of the JSON line plus its delimiter, never a partial write;
/// `emit_ack` performs one ACK attempt with the sequence and identity received.
typedef struct
{
    bool (*emit_event)(void *ctx, const report_data_input_t *input);
    void (*emit_ack)(void *ctx, const report_data_input_t *input);
    void *ctx;
} report_data_effects_t;

/// @brief Empty the window of one registry slot. Used when the slot starts to
/// represent another address or device_id; idempotent pairing preserves it.
void report_dedup_window_reset(size_t registry_index);

/// @brief Empty every window. The windows are born empty at boot and never
/// belong to the persisted blob.
void report_dedup_window_reset_all(void);

/// @brief Number of fingerprints currently held by a slot, for tests and logs.
size_t report_dedup_window_count(size_t registry_index);

/// @brief Whether a fingerprint is currently in the slot window, for tests.
bool report_dedup_window_contains(size_t registry_index,
                                  const report_fingerprint_t *fingerprint);

/// @brief Apply the section 8.2 order for one received DATA frame.
///
/// Lookup, then event, then cache insertion, then ACK: an identity is only
/// recorded after the local acceptance of its event succeeds, and the ACK is
/// only attempted after the insertion.
report_data_outcome_t report_data_policy_process(
    device_registry_state_t registry_state,
    bool join_window_open,
    bool origin_known,
    size_t registry_index,
    const report_data_input_t *input,
    const report_data_effects_t *effects);
