/* Deduplication window and DATA ordering of the coordinator.
 *
 * Covers REPORT-ID-AC-006 (deduplicated retry), AC-007 (local queue without
 * waiting), AC-008 (failure after acceptance), AC-009 (identity conflict),
 * AC-010 (window and reboot) and the window part of AC-012. The registry
 * association, NVS and limit parts of AC-012 stay with the ESP-IDF test app
 * coordinator_154/test_apps/device_registry_test. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "report_data_policy.h"

static unsigned s_tests;

#define CHECK(condition)                                                                \
    do                                                                                  \
    {                                                                                   \
        if (!(condition))                                                               \
        {                                                                               \
            fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__,           \
                    #condition);                                                        \
            exit(1);                                                                    \
        }                                                                               \
    } while (0)

/* Substituted effects: the local acceptance of the host line and the radio ACK.
   `accept_event` stands for the UART space query and submission outcome. */
typedef struct
{
    bool accept_event;
    unsigned events;
    unsigned acks;
    uint16_t last_ack_seq;
    uint64_t last_ack_report_id;
} recorder_t;

static bool record_event(void *ctx, const report_data_input_t *input)
{
    recorder_t *recorder = (recorder_t *)ctx;
    (void)input;
    if (!recorder->accept_event)
    {
        return false;
    }
    ++recorder->events;
    return true;
}

static void record_ack(void *ctx, const report_data_input_t *input)
{
    recorder_t *recorder = (recorder_t *)ctx;
    ++recorder->acks;
    recorder->last_ack_seq = input->seq;
    recorder->last_ack_report_id = input->report_id;
}

static report_data_input_t report(uint64_t report_id, uint16_t seq, uint8_t value)
{
    const report_data_input_t input = {
        .device_id = 0x15400002u,
        .report_id = report_id,
        .seq = seq,
        .endpoint_id = 1,
        .event_type = 2,
        .value = value,
    };
    return input;
}

static report_data_outcome_t process(recorder_t *recorder,
                                     device_registry_state_t state,
                                     bool join_window_open,
                                     bool origin_known,
                                     size_t index,
                                     const report_data_input_t *input)
{
    const report_data_effects_t effects = {
        .emit_event = &record_event,
        .emit_ack = &record_ack,
        .ctx = recorder,
    };
    return report_data_policy_process(state, join_window_open, origin_known, index, input,
                                      &effects);
}

/* AC-006: two receptions of the same fingerprint produce one local event and
   two ACK attempts; the second uses the sequence received with it. */
static void retry_is_deduplicated_and_reacked(void)
{
    report_dedup_window_reset_all();
    recorder_t recorder = {.accept_event = true};

    const report_data_input_t first = report(0xAAAAull, 10, 1);
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &first) ==
          REPORT_DATA_OUTCOME_NEW_ACCEPTED);
    CHECK(recorder.events == 1 && recorder.acks == 1);
    CHECK(recorder.last_ack_seq == 10);

    /* The retry carries a new sequence and the same identity. */
    const report_data_input_t retry = report(0xAAAAull, 77, 1);
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &retry) ==
          REPORT_DATA_OUTCOME_RETRY_DEDUPLICATED);
    CHECK(recorder.events == 1);
    CHECK(recorder.acks == 2);
    CHECK(recorder.last_ack_seq == 77);
    CHECK(recorder.last_ack_report_id == 0xAAAAull);
    ++s_tests;
}

/* The defect this specification removes: a sequence restarted by a client
   reboot no longer suppresses a legitimate report. */
static void restarted_sequence_no_longer_suppresses_a_report(void)
{
    report_dedup_window_reset_all();
    recorder_t recorder = {.accept_event = true};

    const report_data_input_t before = report(0x1111ull, 0, 1);
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &before) ==
          REPORT_DATA_OUTCOME_NEW_ACCEPTED);

    /* Same sequence, new boot, new identity. */
    const report_data_input_t after = report(0x2222ull, 0, 1);
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &after) ==
          REPORT_DATA_OUTCOME_NEW_ACCEPTED);
    CHECK(recorder.events == 2 && recorder.acks == 2);
    ++s_tests;
}

/* AC-007: a local acceptance that fails inserts no identity and sends no ACK,
   and a retry after recovery tries the event again. */
static void unavailable_local_acceptance_neither_caches_nor_acks(void)
{
    report_dedup_window_reset_all();
    recorder_t recorder = {.accept_event = false};

    const report_data_input_t input = report(0xBBBBull, 5, 1);
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &input) ==
          REPORT_DATA_OUTCOME_NEW_LOCAL_UNAVAILABLE);
    CHECK(recorder.events == 0);
    CHECK(recorder.acks == 0);
    CHECK(report_dedup_window_count(0) == 0);

    recorder.accept_event = true;
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &input) ==
          REPORT_DATA_OUTCOME_NEW_ACCEPTED);
    CHECK(recorder.events == 1 && recorder.acks == 1);
    CHECK(report_dedup_window_count(0) == 1);
    ++s_tests;
}

/* AC-008: an ACK lost after the event and the cache makes the next retry
   receive an ACK without a second event. */
static void failure_after_acceptance_is_reacked_without_a_second_event(void)
{
    report_dedup_window_reset_all();
    recorder_t recorder = {.accept_event = true};

    const report_data_input_t input = report(0xCCCCull, 9, 1);
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &input) ==
          REPORT_DATA_OUTCOME_NEW_ACCEPTED);
    /* The ACK attempt happened; the transmission is assumed lost in flight. */
    CHECK(recorder.events == 1 && recorder.acks == 1);

    const report_data_input_t retry = report(0xCCCCull, 12, 1);
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &retry) ==
          REPORT_DATA_OUTCOME_RETRY_DEDUPLICATED);
    CHECK(recorder.events == 1);
    CHECK(recorder.acks == 2);
    ++s_tests;
}

/* AC-009: the same identity with different content produces no event, no ACK
   and no window change. */
static void identity_conflict_produces_nothing(void)
{
    report_dedup_window_reset_all();
    recorder_t recorder = {.accept_event = true};

    const report_data_input_t original = report(0xDDDDull, 1, 1);
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &original) ==
          REPORT_DATA_OUTCOME_NEW_ACCEPTED);

    const report_data_input_t conflicting = report(0xDDDDull, 2, 0);
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &conflicting) ==
          REPORT_DATA_OUTCOME_CONFLICT);
    CHECK(recorder.events == 1);
    CHECK(recorder.acks == 1);
    CHECK(report_dedup_window_count(0) == 1);

    /* The original fingerprint survives the conflict and is still a retry. */
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &original) ==
          REPORT_DATA_OUTCOME_RETRY_DEDUPLICATED);
    ++s_tests;
}

/* AC-010: nine identities demonstrate a FIFO of eight and the eviction of the
   oldest; an evicted identity may generate an event again. */
static void window_is_a_fifo_of_eight(void)
{
    report_dedup_window_reset_all();
    recorder_t recorder = {.accept_event = true};

    for (uint64_t identity = 1; identity <= REPORT_DEDUP_WINDOW_CAPACITY; ++identity)
    {
        const report_data_input_t input = report(identity, (uint16_t)identity, 1);
        CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &input) ==
              REPORT_DATA_OUTCOME_NEW_ACCEPTED);
    }
    CHECK(report_dedup_window_count(0) == REPORT_DEDUP_WINDOW_CAPACITY);
    CHECK(recorder.events == REPORT_DEDUP_WINDOW_CAPACITY);

    /* A retry does not reorder its entry. */
    const report_data_input_t oldest = report(1, 100, 1);
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &oldest) ==
          REPORT_DATA_OUTCOME_RETRY_DEDUPLICATED);

    /* The ninth identity evicts the oldest. */
    const report_data_input_t ninth = report(9, 9, 1);
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &ninth) ==
          REPORT_DATA_OUTCOME_NEW_ACCEPTED);
    CHECK(report_dedup_window_count(0) == REPORT_DEDUP_WINDOW_CAPACITY);

    const report_fingerprint_t evicted = {
        .report_id = 1, .endpoint_id = 1, .event_type = 2, .value = 1};
    CHECK(!report_dedup_window_contains(0, &evicted));
    const report_fingerprint_t second = {
        .report_id = 2, .endpoint_id = 1, .event_type = 2, .value = 1};
    CHECK(report_dedup_window_contains(0, &second));

    /* The evicted identity is accepted again and generates a repeated event.
       This is the declared limit of a volatile, finite window. */
    const unsigned events_before = recorder.events;
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &oldest) ==
          REPORT_DATA_OUTCOME_NEW_ACCEPTED);
    CHECK(recorder.events == events_before + 1);
    ++s_tests;
}

/* AC-010 and AC-012: a simulated reboot empties every window, and a real
   identity change clears the affected slot while other slots are untouched. */
static void reboot_and_identity_change_clear_the_window(void)
{
    report_dedup_window_reset_all();
    recorder_t recorder = {.accept_event = true};

    const report_data_input_t first = report(0xEEEEull, 1, 1);
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &first) ==
          REPORT_DATA_OUTCOME_NEW_ACCEPTED);
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 3, &first) ==
          REPORT_DATA_OUTCOME_NEW_ACCEPTED);
    CHECK(report_dedup_window_count(0) == 1 && report_dedup_window_count(3) == 1);

    /* The slot starts to represent another identity: only its window is cleared. */
    report_dedup_window_reset(0);
    CHECK(report_dedup_window_count(0) == 0);
    CHECK(report_dedup_window_count(3) == 1);
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 0, &first) ==
          REPORT_DATA_OUTCOME_NEW_ACCEPTED);

    /* A coordinator reboot: every window is born empty again. */
    report_dedup_window_reset_all();
    CHECK(report_dedup_window_count(0) == 0 && report_dedup_window_count(3) == 0);
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, true, 3, &first) ==
          REPORT_DATA_OUTCOME_NEW_ACCEPTED);
    ++s_tests;
}

/* AC-012: the registry policy in force remains the input of the decision, for
   an unavailable registry and for an unknown origin. */
static void registry_policy_still_governs_availability_and_unknown_origin(void)
{
    report_dedup_window_reset_all();
    recorder_t recorder = {.accept_event = true};
    const report_data_input_t input = report(0x4242ull, 1, 1);

    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_UNAVAILABLE, true, true, 0, &input) ==
          REPORT_DATA_OUTCOME_REGISTRY_UNAVAILABLE);
    CHECK(recorder.events == 0 && recorder.acks == 0);
    CHECK(report_dedup_window_count(0) == 0);

    /* Unknown origin with the window closed keeps the existing rejection. */
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, false, false, 0, &input) ==
          REPORT_DATA_OUTCOME_UNKNOWN_IGNORED);
    CHECK(recorder.events == 0 && recorder.acks == 0);

    /* Unknown origin with the window open is forwarded and ACKed, with no
       deduplication promise and no dedup slot of its own. */
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, true, false, 0, &input) ==
          REPORT_DATA_OUTCOME_UNKNOWN_FORWARDED);
    CHECK(recorder.events == 1 && recorder.acks == 1);
    CHECK(report_dedup_window_count(0) == 0);

    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, true, false, 0, &input) ==
          REPORT_DATA_OUTCOME_UNKNOWN_FORWARDED);
    CHECK(recorder.events == 2 && recorder.acks == 2);

    /* Invariant 10 also holds for an unknown origin: a failed local acceptance
       is never converted into a successful ACK. */
    recorder.accept_event = false;
    CHECK(process(&recorder, DEVICE_REGISTRY_STATE_READY, true, false, 0, &input) ==
          REPORT_DATA_OUTCOME_UNKNOWN_LOCAL_UNAVAILABLE);
    CHECK(recorder.events == 2 && recorder.acks == 2);
    ++s_tests;
}

int main(void)
{
    retry_is_deduplicated_and_reacked();
    restarted_sequence_no_longer_suppresses_a_report();
    unavailable_local_acceptance_neither_caches_nor_acks();
    failure_after_acceptance_is_reacked_without_a_second_event();
    identity_conflict_produces_nothing();
    window_is_a_fifo_of_eight();
    reboot_and_identity_change_clear_the_window();
    registry_policy_still_governs_availability_and_unknown_origin();
    printf("%u Tests 0 Failures 0 Ignored\n", s_tests);
    return 0;
}
