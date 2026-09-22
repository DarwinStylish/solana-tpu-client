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

struct solana_delivery_client {
    solana_delivery_allocator_t allocator;
    solana_delivery_monotonic_time_fn monotonic_time_fn;
    void *monotonic_time_context;
    bool has_topology;
    solana_delivery_owned_topology_t topology;
};

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

#endif
