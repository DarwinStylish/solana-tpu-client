// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/delivery.h"

#include <stddef.h>
#include <stdint.h>

_Static_assert(SOLANA_DELIVERY_ABI_VERSION == UINT32_C(0x00010000),
               "delivery ABI version changed");
_Static_assert(sizeof(solana_delivery_status_t) == 4,
               "delivery status ABI changed");
_Static_assert(sizeof(solana_delivery_request_id_t) == 8,
               "request id ABI changed");
_Static_assert(sizeof(solana_delivery_attempt_id_t) == 8,
               "attempt id ABI changed");
_Static_assert(sizeof(solana_delivery_validator_identity_t) == 32,
               "validator identity ABI changed");

_Static_assert(sizeof(solana_delivery_endpoint_t) == 32,
               "endpoint ABI changed");
_Static_assert(offsetof(solana_delivery_endpoint_t, struct_size) == 0,
               "endpoint struct_size offset changed");
_Static_assert(offsetof(solana_delivery_endpoint_t, address_family) == 4,
               "endpoint family offset changed");
_Static_assert(offsetof(solana_delivery_endpoint_t, transport) == 5,
               "endpoint transport offset changed");
_Static_assert(offsetof(solana_delivery_endpoint_t, role) == 6,
               "endpoint role offset changed");
_Static_assert(offsetof(solana_delivery_endpoint_t, port) == 8,
               "endpoint port offset changed");
_Static_assert(offsetof(solana_delivery_endpoint_t, address) == 12,
               "endpoint address offset changed");

_Static_assert(sizeof(solana_delivery_validator_t) == 48,
               "validator ABI changed");
_Static_assert(offsetof(solana_delivery_validator_t, struct_size) == 0,
               "validator struct_size offset changed");
_Static_assert(offsetof(solana_delivery_validator_t, identity) == 8,
               "validator identity offset changed");

_Static_assert(sizeof(solana_delivery_validator_endpoint_t) == 16,
               "validator endpoint ABI changed");
_Static_assert(sizeof(solana_delivery_leader_t) == 32,
               "leader ABI changed");
_Static_assert(sizeof(solana_delivery_submit_options_t) == 24,
               "submit options ABI changed");
_Static_assert(
    offsetof(solana_delivery_submit_options_t, struct_size) == 0,
    "submit options struct_size offset changed"
);
_Static_assert(
    offsetof(solana_delivery_submit_options_t, flags) == 4,
    "submit options flags offset changed"
);
_Static_assert(
    offsetof(solana_delivery_submit_options_t, max_topology_age_ns) == 8,
    "submit options topology age offset changed"
);
_Static_assert(
    offsetof(solana_delivery_submit_options_t, target_limit) == 16,
    "submit options target limit offset changed"
);
_Static_assert(
    offsetof(solana_delivery_submit_options_t, reserved0) == 20,
    "submit options reserved offset changed"
);
_Static_assert(
    sizeof(((solana_delivery_submit_options_t *)0)->target_limit) == 4,
    "submit options target limit width changed"
);

_Static_assert(offsetof(solana_delivery_topology_t, struct_size) == 0,
               "topology struct_size offset changed");
_Static_assert(offsetof(solana_delivery_topology_t, generation) == 8,
               "topology generation offset changed");
_Static_assert(offsetof(solana_delivery_topology_t, current_slot) == 16,
               "topology current_slot offset changed");
_Static_assert(
    sizeof(((solana_delivery_topology_t *)0)->validator_stride) == 4,
    "validator stride ABI changed"
);
_Static_assert(
    sizeof(((solana_delivery_topology_t *)0)->endpoint_stride) == 4,
    "endpoint stride ABI changed"
);
_Static_assert(
    sizeof(((solana_delivery_topology_t *)0)->validator_endpoint_stride) == 4,
    "validator-endpoint stride ABI changed"
);
_Static_assert(
    sizeof(((solana_delivery_topology_t *)0)->leader_stride) == 4,
    "leader stride ABI changed"
);

_Static_assert(sizeof(solana_delivery_event_t) == 64,
               "delivery event ABI changed");
_Static_assert(offsetof(solana_delivery_event_t, struct_size) == 0,
               "event struct_size offset changed");
_Static_assert(offsetof(solana_delivery_event_t, event_class) == 4,
               "event class offset changed");
_Static_assert(offsetof(solana_delivery_event_t, event_code) == 8,
               "event code offset changed");
_Static_assert(offsetof(solana_delivery_event_t, diagnostic_code) == 12,
               "event diagnostic offset changed");
_Static_assert(offsetof(solana_delivery_event_t, request_id) == 16,
               "event request id offset changed");
_Static_assert(offsetof(solana_delivery_event_t, attempt_id) == 24,
               "event attempt id offset changed");
_Static_assert(offsetof(solana_delivery_event_t, request_sequence) == 32,
               "event sequence offset changed");
_Static_assert(offsetof(solana_delivery_event_t, monotonic_time_ns) == 40,
               "event timestamp offset changed");

int main(void) {
    return 0;
}
