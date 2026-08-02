#include "device_registry_policy.h"

device_registry_discovery_action_t device_registry_policy_discovery(
    device_registry_state_t registry_state,
    bool join_window_open)
{
    if (registry_state != DEVICE_REGISTRY_STATE_READY)
    {
        return DEVICE_REGISTRY_DISCOVERY_REJECT_UNAVAILABLE;
    }
    if (!join_window_open)
    {
        return DEVICE_REGISTRY_DISCOVERY_REJECT_WINDOW_CLOSED;
    }
    return DEVICE_REGISTRY_DISCOVERY_PROCESS;
}

bool device_registry_policy_discovery_response(device_registry_pair_result_t pair_result)
{
    return pair_result == DEVICE_REGISTRY_PAIR_KNOWN ||
           pair_result == DEVICE_REGISTRY_PAIR_UPDATED ||
           pair_result == DEVICE_REGISTRY_PAIR_CREATED;
}

device_registry_data_effects_t device_registry_policy_data(
    device_registry_state_t registry_state,
    bool join_window_open,
    bool origin_known,
    bool duplicate)
{
    device_registry_data_effects_t effects = {0};
    if (registry_state != DEVICE_REGISTRY_STATE_READY)
    {
        effects.log_registry_unavailable = true;
        return effects;
    }
    if (!origin_known)
    {
        if (!join_window_open)
        {
            effects.log_unknown_device = true;
            return effects;
        }

        /* Section 9 deliberately leaves operational acceptance open while the window is open.
           Preserve the existing event+ACK behavior without creating a registry write. */
        effects.emit_host_event = true;
        effects.emit_ack = true;
        return effects;
    }

    effects.emit_host_event = !duplicate;
    effects.emit_ack = true;
    return effects;
}

device_registry_ack_action_t device_registry_policy_ack(
    device_registry_state_t registry_state,
    bool origin_known,
    bool pending_correlations_match)
{
    if (registry_state != DEVICE_REGISTRY_STATE_READY)
    {
        return DEVICE_REGISTRY_ACK_REJECT_UNAVAILABLE;
    }
    return origin_known && pending_correlations_match
               ? DEVICE_REGISTRY_ACK_COMPLETE
               : DEVICE_REGISTRY_ACK_IGNORE;
}

device_registry_host_command_action_t device_registry_policy_host_command(
    bool address_valid,
    device_registry_state_t registry_state,
    bool pending_command,
    bool target_known)
{
    /* Section 5.1: input validity, registry availability, then operational correlation/identity. */
    if (!address_valid)
    {
        return DEVICE_REGISTRY_HOST_COMMAND_INVALID_ADDRESS;
    }
    if (registry_state != DEVICE_REGISTRY_STATE_READY)
    {
        return DEVICE_REGISTRY_HOST_COMMAND_REGISTRY_UNAVAILABLE;
    }
    if (pending_command)
    {
        return DEVICE_REGISTRY_HOST_COMMAND_PENDING;
    }
    if (!target_known)
    {
        return DEVICE_REGISTRY_HOST_COMMAND_UNKNOWN_DEVICE;
    }
    return DEVICE_REGISTRY_HOST_COMMAND_START;
}
