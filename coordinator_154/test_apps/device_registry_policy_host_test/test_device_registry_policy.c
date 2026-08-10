#include <stdio.h>
#include <stdlib.h>

#include "device_registry_policy.h"

static unsigned s_tests;

#define CHECK(condition)                                                                    \
    do                                                                                      \
    {                                                                                       \
        if (!(condition))                                                                   \
        {                                                                                   \
            fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            exit(1);                                                                        \
        }                                                                                   \
    } while (0)

static void discovery_precedence(void)
{
    CHECK(device_registry_policy_discovery(DEVICE_REGISTRY_STATE_UNAVAILABLE, true) ==
           DEVICE_REGISTRY_DISCOVERY_REJECT_UNAVAILABLE);
    CHECK(device_registry_policy_discovery(DEVICE_REGISTRY_STATE_UNAVAILABLE, false) ==
           DEVICE_REGISTRY_DISCOVERY_REJECT_UNAVAILABLE);
    CHECK(device_registry_policy_discovery(DEVICE_REGISTRY_STATE_READY, false) ==
           DEVICE_REGISTRY_DISCOVERY_REJECT_WINDOW_CLOSED);
    CHECK(device_registry_policy_discovery(DEVICE_REGISTRY_STATE_READY, true) ==
           DEVICE_REGISTRY_DISCOVERY_PROCESS);
    ++s_tests;
}

static void discovery_response_after_pairing(void)
{
    CHECK(device_registry_policy_discovery_response(DEVICE_REGISTRY_PAIR_KNOWN));
    CHECK(device_registry_policy_discovery_response(DEVICE_REGISTRY_PAIR_UPDATED));
    CHECK(device_registry_policy_discovery_response(DEVICE_REGISTRY_PAIR_CREATED));
    CHECK(!device_registry_policy_discovery_response(DEVICE_REGISTRY_PAIR_REJECTED_FULL));
    CHECK(!device_registry_policy_discovery_response(DEVICE_REGISTRY_PAIR_FAILED));
    ++s_tests;
}

static void unavailable_data_fails_closed(void)
{
    const device_registry_data_effects_t effects = device_registry_policy_data(
        DEVICE_REGISTRY_STATE_UNAVAILABLE, true, true, false);
    CHECK(!effects.emit_host_event);
    CHECK(!effects.emit_ack);
    CHECK(!effects.log_unknown_device);
    CHECK(effects.log_registry_unavailable);
    ++s_tests;
}

static void unknown_data_obeys_window_without_pairing_effect(void)
{
    device_registry_data_effects_t effects = device_registry_policy_data(
        DEVICE_REGISTRY_STATE_READY, false, false, false);
    CHECK(!effects.emit_host_event && !effects.emit_ack && effects.log_unknown_device);

    effects = device_registry_policy_data(DEVICE_REGISTRY_STATE_READY, true, false, false);
    CHECK(effects.emit_host_event && effects.emit_ack);
    CHECK(!effects.log_unknown_device && !effects.log_registry_unavailable);
    ++s_tests;
}

static void known_data_preserves_dedup_effects(void)
{
    device_registry_data_effects_t effects = device_registry_policy_data(
        DEVICE_REGISTRY_STATE_READY, false, true, false);
    CHECK(effects.emit_host_event && effects.emit_ack);

    effects = device_registry_policy_data(DEVICE_REGISTRY_STATE_READY, false, true, true);
    CHECK(!effects.emit_host_event && effects.emit_ack);
    ++s_tests;
}

static void ack_requires_registry_identity_and_correlation(void)
{
    CHECK(device_registry_policy_ack(DEVICE_REGISTRY_STATE_UNAVAILABLE, true, true) ==
           DEVICE_REGISTRY_ACK_REJECT_UNAVAILABLE);
    CHECK(device_registry_policy_ack(DEVICE_REGISTRY_STATE_READY, false, true) ==
           DEVICE_REGISTRY_ACK_IGNORE);
    CHECK(device_registry_policy_ack(DEVICE_REGISTRY_STATE_READY, true, false) ==
           DEVICE_REGISTRY_ACK_IGNORE);
    CHECK(device_registry_policy_ack(DEVICE_REGISTRY_STATE_READY, true, true) ==
           DEVICE_REGISTRY_ACK_COMPLETE);
    ++s_tests;
}

static void host_command_uses_normative_precedence(void)
{
    CHECK(device_registry_policy_host_command(false, DEVICE_REGISTRY_STATE_UNAVAILABLE, true, false) ==
           DEVICE_REGISTRY_HOST_COMMAND_INVALID_ADDRESS);
    CHECK(device_registry_policy_host_command(true, DEVICE_REGISTRY_STATE_UNAVAILABLE, true, false) ==
           DEVICE_REGISTRY_HOST_COMMAND_REGISTRY_UNAVAILABLE);
    CHECK(device_registry_policy_host_command(true, DEVICE_REGISTRY_STATE_READY, true, true) ==
           DEVICE_REGISTRY_HOST_COMMAND_PENDING);
    CHECK(device_registry_policy_host_command(true, DEVICE_REGISTRY_STATE_READY, false, false) ==
           DEVICE_REGISTRY_HOST_COMMAND_UNKNOWN_DEVICE);
    CHECK(device_registry_policy_host_command(true, DEVICE_REGISTRY_STATE_READY, false, true) ==
           DEVICE_REGISTRY_HOST_COMMAND_START);
    ++s_tests;
}

int main(void)
{
    discovery_precedence();
    discovery_response_after_pairing();
    unavailable_data_fails_closed();
    unknown_data_obeys_window_without_pairing_effect();
    known_data_preserves_dedup_effects();
    ack_requires_registry_identity_and_correlation();
    host_command_uses_normative_precedence();
    printf("%u Tests 0 Failures 0 Ignored\n", s_tests);
    return 0;
}
