// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#ifndef SOLANA_DELIVERY_INTERNAL_H
#define SOLANA_DELIVERY_INTERNAL_H

#include "solana/delivery.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void *(*solana_delivery_calloc_fn)(
    void *context,
    size_t count,
    size_t size
);

typedef void (*solana_delivery_free_fn)(
    void *context,
    void *pointer
);

typedef struct {
    solana_delivery_calloc_fn calloc_fn;
    solana_delivery_free_fn free_fn;
    void *context;
} solana_delivery_allocator_t;

typedef solana_delivery_status_t
(*solana_delivery_monotonic_time_fn)(
    void *context,
    uint64_t *out_time_ns
);

typedef struct {
    solana_delivery_topology_t view;
    solana_delivery_validator_t *validators;
    solana_delivery_endpoint_t *endpoints;
    solana_delivery_validator_endpoint_t *validator_endpoints;
    solana_delivery_leader_t *leaders;
    uint64_t received_monotonic_ns;
} solana_delivery_owned_topology_t;

typedef struct {
    uint32_t leader_index;
    uint32_t validator_index;
    uint32_t endpoint_index;
    solana_delivery_validator_identity_t validator_identity;
    solana_delivery_endpoint_t endpoint;
} solana_delivery_owned_request_target_t;

typedef uint32_t solana_delivery_request_state_t;

#define SOLANA_DELIVERY_REQUEST_STATE_ACCEPTED UINT32_C(1)

#define SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY 64U

typedef struct solana_delivery_owned_request {
    solana_delivery_request_id_t request_id;
    uint64_t topology_generation;
    uint64_t routing_slot;
    uint8_t *transaction_bytes;
    size_t transaction_length;
    solana_delivery_owned_request_target_t *targets;
    size_t target_count;
    solana_delivery_request_state_t state;
    uint64_t next_event_sequence;
    struct solana_delivery_owned_request *next;
} solana_delivery_owned_request_t;

typedef struct {
    solana_delivery_event_t
        entries[SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY];
    size_t head;
    size_t count;
} solana_delivery_event_queue_t;

struct solana_delivery_client {
    solana_delivery_allocator_t allocator;
    solana_delivery_monotonic_time_fn monotonic_time_fn;
    void *monotonic_time_context;
    bool has_topology;
    solana_delivery_owned_topology_t topology;
    solana_delivery_request_id_t next_request_id;
    solana_delivery_owned_request_t *request_head;
    solana_delivery_event_queue_t event_queue;
};

bool solana_delivery_event_queue_has_capacity(
    const solana_delivery_client_t *client
);

solana_delivery_status_t solana_delivery_event_queue_push(
    solana_delivery_client_t *client,
    const solana_delivery_event_t *event
);

solana_delivery_status_t
solana_delivery_client_create_with_dependencies(
    const solana_delivery_allocator_t *allocator,
    solana_delivery_monotonic_time_fn monotonic_time_fn,
    void *monotonic_time_context,
    solana_delivery_client_t **out_client
);

solana_delivery_status_t
solana_delivery_client_create_with_allocator(
    const solana_delivery_allocator_t *allocator,
    solana_delivery_client_t **out_client
);

typedef struct {
    uint64_t max_topology_age_ns;
    size_t target_limit;
} solana_delivery_submission_policy_t;

solana_delivery_status_t
solana_delivery_client_evaluate_submission_policy(
    const solana_delivery_client_t *client,
    const solana_delivery_submission_policy_t *policy
);

typedef struct {
    uint32_t leader_index;
    uint32_t validator_index;
    uint32_t endpoint_index;
} solana_delivery_topology_candidate_t;

solana_delivery_status_t
solana_delivery_client_resolve_slot(
    const solana_delivery_client_t *client,
    uint64_t slot,
    solana_delivery_topology_candidate_t *candidates,
    size_t capacity,
    size_t *out_count
);

typedef struct {
    uint32_t leader_index;
    uint32_t validator_index;
    uint32_t endpoint_index;
} solana_delivery_route_target_t;

solana_delivery_status_t
solana_delivery_plan_routes(
    const solana_delivery_topology_candidate_t *candidates,
    size_t candidate_count,
    size_t target_limit,
    solana_delivery_route_target_t *targets,
    size_t capacity,
    size_t *out_target_count,
    size_t *out_unique_target_count
);

#endif
