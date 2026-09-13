// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#ifndef SOLANA_DELIVERY_INTERNAL_H
#define SOLANA_DELIVERY_INTERNAL_H

#include "solana/delivery.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    solana_delivery_topology_t view;
    solana_delivery_validator_t *validators;
    solana_delivery_endpoint_t *endpoints;
    solana_delivery_validator_endpoint_t *validator_endpoints;
    solana_delivery_leader_t *leaders;
    uint64_t received_monotonic_ns;
} solana_delivery_owned_topology_t;

struct solana_delivery_client {
    bool has_topology;
    solana_delivery_owned_topology_t topology;
};

#endif
