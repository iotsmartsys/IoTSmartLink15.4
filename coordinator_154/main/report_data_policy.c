#include "report_data_policy.h"

/// Volatile per-slot FIFO windows. Static storage, no allocation and no I/O on
/// the insertion path; zero-initialized, so every window is born empty at boot.
static report_fingerprint_t s_window[DEVICE_REGISTRY_MAX_ENTRIES][REPORT_DEDUP_WINDOW_CAPACITY];
static uint8_t s_window_count[DEVICE_REGISTRY_MAX_ENTRIES];
static uint8_t s_window_oldest[DEVICE_REGISTRY_MAX_ENTRIES];

typedef enum
{
    WINDOW_LOOKUP_ABSENT = 0,
    WINDOW_LOOKUP_SAME_FINGERPRINT,
    WINDOW_LOOKUP_CONFLICT,
} window_lookup_t;

static bool window_index_is_valid(size_t registry_index)
{
    return registry_index < DEVICE_REGISTRY_MAX_ENTRIES;
}

static window_lookup_t window_lookup(size_t registry_index, const report_data_input_t *input)
{
    const uint8_t count = s_window_count[registry_index];
    for (uint8_t position = 0; position < count; ++position)
    {
        const uint8_t slot =
            (uint8_t)((s_window_oldest[registry_index] + position) % REPORT_DEDUP_WINDOW_CAPACITY);
        const report_fingerprint_t *entry = &s_window[registry_index][slot];
        if (entry->report_id != input->report_id)
        {
            continue;
        }
        /* Same identity with different content is a conflict, not a retry. */
        if (entry->endpoint_id == input->endpoint_id &&
            entry->event_type == input->event_type &&
            entry->value == input->value)
        {
            return WINDOW_LOOKUP_SAME_FINGERPRINT;
        }
        return WINDOW_LOOKUP_CONFLICT;
    }
    return WINDOW_LOOKUP_ABSENT;
}

/// Append a fingerprint, evicting the oldest entry when the ninth arrives. A
/// retry never reorders an entry, so it is only ever called for a new identity.
static void window_insert(size_t registry_index, const report_data_input_t *input)
{
    const report_fingerprint_t fingerprint = {
        .report_id = input->report_id,
        .endpoint_id = input->endpoint_id,
        .event_type = input->event_type,
        .value = input->value,
    };

    if (s_window_count[registry_index] < REPORT_DEDUP_WINDOW_CAPACITY)
    {
        const uint8_t slot = (uint8_t)((s_window_oldest[registry_index] +
                                        s_window_count[registry_index]) %
                                       REPORT_DEDUP_WINDOW_CAPACITY);
        s_window[registry_index][slot] = fingerprint;
        ++s_window_count[registry_index];
        return;
    }

    s_window[registry_index][s_window_oldest[registry_index]] = fingerprint;
    s_window_oldest[registry_index] =
        (uint8_t)((s_window_oldest[registry_index] + 1U) % REPORT_DEDUP_WINDOW_CAPACITY);
}

void report_dedup_window_reset(size_t registry_index)
{
    if (!window_index_is_valid(registry_index))
    {
        return;
    }
    s_window_count[registry_index] = 0;
    s_window_oldest[registry_index] = 0;
}

void report_dedup_window_reset_all(void)
{
    for (size_t index = 0; index < DEVICE_REGISTRY_MAX_ENTRIES; ++index)
    {
        report_dedup_window_reset(index);
    }
}

size_t report_dedup_window_count(size_t registry_index)
{
    return window_index_is_valid(registry_index) ? s_window_count[registry_index] : 0;
}

bool report_dedup_window_contains(size_t registry_index,
                                  const report_fingerprint_t *fingerprint)
{
    if (!window_index_is_valid(registry_index) || fingerprint == NULL)
    {
        return false;
    }

    const report_data_input_t probe = {
        .report_id = fingerprint->report_id,
        .endpoint_id = fingerprint->endpoint_id,
        .event_type = fingerprint->event_type,
        .value = fingerprint->value,
    };
    return window_lookup(registry_index, &probe) == WINDOW_LOOKUP_SAME_FINGERPRINT;
}

report_data_outcome_t report_data_policy_process(
    device_registry_state_t registry_state,
    bool join_window_open,
    bool origin_known,
    size_t registry_index,
    const report_data_input_t *input,
    const report_data_effects_t *effects)
{
    if (input == NULL || effects == NULL || effects->emit_event == NULL ||
        effects->emit_ack == NULL)
    {
        return REPORT_DATA_OUTCOME_REGISTRY_UNAVAILABLE;
    }

    /* An identity of zero never reaches here: the codec rejects such a DATA
       frame as invalid before the receive path dispatches it. */
    const bool known_slot = origin_known && window_index_is_valid(registry_index);

    /* The registry policy in force remains the input of this decision: it owns
       availability, the join window and the treatment of an unknown origin. */
    window_lookup_t lookup = WINDOW_LOOKUP_ABSENT;
    if (known_slot)
    {
        lookup = window_lookup(registry_index, input);
    }

    const device_registry_data_effects_t registry_effects = device_registry_policy_data(
        registry_state,
        join_window_open,
        origin_known,
        lookup == WINDOW_LOOKUP_SAME_FINGERPRINT);

    if (registry_effects.log_registry_unavailable)
    {
        return REPORT_DATA_OUTCOME_REGISTRY_UNAVAILABLE;
    }
    if (registry_effects.log_unknown_device)
    {
        return REPORT_DATA_OUTCOME_UNKNOWN_IGNORED;
    }

    if (lookup == WINDOW_LOOKUP_CONFLICT)
    {
        /* No event, no window mutation and no ACK. */
        return REPORT_DATA_OUTCOME_CONFLICT;
    }

    if (lookup == WINDOW_LOOKUP_SAME_FINGERPRINT)
    {
        /* A cached identity is ACKable again without a second event, using the
           sequence and identity of the attempt just received. */
        effects->emit_ack(effects->ctx, input);
        return REPORT_DATA_OUTCOME_RETRY_DEDUPLICATED;
    }

    if (!registry_effects.emit_host_event)
    {
        return REPORT_DATA_OUTCOME_REGISTRY_UNAVAILABLE;
    }

    /* New identity: the event must be accepted locally and in full before the
       identity is cached, and the ACK only follows the insertion. A failure is
       never converted into a successful ACK. */
    if (!effects->emit_event(effects->ctx, input))
    {
        return known_slot ? REPORT_DATA_OUTCOME_NEW_LOCAL_UNAVAILABLE
                          : REPORT_DATA_OUTCOME_UNKNOWN_LOCAL_UNAVAILABLE;
    }

    if (known_slot)
    {
        window_insert(registry_index, input);
    }

    if (registry_effects.emit_ack)
    {
        effects->emit_ack(effects->ctx, input);
    }
    return known_slot ? REPORT_DATA_OUTCOME_NEW_ACCEPTED
                      : REPORT_DATA_OUTCOME_UNKNOWN_FORWARDED;
}
