#pragma once

#include <stdbool.h>

#include "device_registry.h"

typedef enum
{
    DEVICE_REGISTRY_DISCOVERY_PROCESS = 0,
    DEVICE_REGISTRY_DISCOVERY_REJECT_WINDOW_CLOSED,
    DEVICE_REGISTRY_DISCOVERY_REJECT_UNAVAILABLE,
} device_registry_discovery_action_t;

typedef struct
{
    bool emit_host_event;
    bool emit_ack;
    bool log_unknown_device;
    bool log_registry_unavailable;
} device_registry_data_effects_t;

typedef enum
{
    DEVICE_REGISTRY_ACK_IGNORE = 0,
    DEVICE_REGISTRY_ACK_COMPLETE,
    DEVICE_REGISTRY_ACK_REJECT_UNAVAILABLE,
} device_registry_ack_action_t;

typedef enum
{
    DEVICE_REGISTRY_HOST_COMMAND_START = 0,
    DEVICE_REGISTRY_HOST_COMMAND_INVALID_ADDRESS,
    DEVICE_REGISTRY_HOST_COMMAND_REGISTRY_UNAVAILABLE,
    DEVICE_REGISTRY_HOST_COMMAND_PENDING,
    DEVICE_REGISTRY_HOST_COMMAND_UNKNOWN_DEVICE,
} device_registry_host_command_action_t;

device_registry_discovery_action_t device_registry_policy_discovery(
    device_registry_state_t registry_state,
    bool join_window_open);

bool device_registry_policy_discovery_response(device_registry_pair_result_t pair_result);

device_registry_data_effects_t device_registry_policy_data(
    device_registry_state_t registry_state,
    bool join_window_open,
    bool origin_known,
    bool duplicate);

device_registry_ack_action_t device_registry_policy_ack(
    device_registry_state_t registry_state,
    bool origin_known,
    bool pending_correlations_match);

device_registry_host_command_action_t device_registry_policy_host_command(
    bool address_valid,
    device_registry_state_t registry_state,
    bool pending_command,
    bool target_known);
