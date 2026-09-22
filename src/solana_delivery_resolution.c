// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana_delivery_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool leader_contains_slot(
    const solana_delivery_leader_t *leader,
    uint64_t slot
) {
    return slot >= leader->first_slot &&
           slot <= leader->last_slot;
}

static solana_delivery_status_t count_slot_candidates(
    const solana_delivery_client_t *client,
    uint64_t slot,
    size_t *out_count
) {
    size_t count = 0U;

    for (uint32_t leader_index = 0U;
         leader_index < client->topology.view.leader_count;
         ++leader_index) {
        const solana_delivery_leader_t *leader =
            &client->topology.leaders[leader_index];

        if (!leader_contains_slot(leader, slot)) {
            continue;
        }

        for (uint32_t association_index = 0U;
             association_index <
                 client->topology.view
                       .validator_endpoint_count;
             ++association_index) {
            const solana_delivery_validator_endpoint_t *
                association =
                    &client->topology
                           .validator_endpoints[
                               association_index
                           ];

            if (association->validator_index !=
                leader->validator_index) {
                continue;
            }

            if (count == SIZE_MAX) {
                return
                    SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED;
            }

            ++count;
        }
    }

    *out_count = count;
    return SOLANA_DELIVERY_STATUS_OK;
}

solana_delivery_status_t
solana_delivery_client_resolve_slot(
    const solana_delivery_client_t *client,
    uint64_t slot,
    solana_delivery_topology_candidate_t *candidates,
    size_t capacity,
    size_t *out_count
) {
    if (out_count == NULL) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    *out_count = 0U;

    if (client == NULL ||
        (capacity != 0U && candidates == NULL)) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if (!client->has_topology) {
        return SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE;
    }

    size_t required = 0U;
    solana_delivery_status_t status =
        count_slot_candidates(
            client,
            slot,
            &required
        );

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        return status;
    }

    if (required == 0U) {
        return SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE;
    }

    *out_count = required;

    if (capacity < required) {
        return SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED;
    }

    size_t output_index = 0U;

    for (uint32_t leader_index = 0U;
         leader_index < client->topology.view.leader_count;
         ++leader_index) {
        const solana_delivery_leader_t *leader =
            &client->topology.leaders[leader_index];

        if (!leader_contains_slot(leader, slot)) {
            continue;
        }

        for (uint32_t association_index = 0U;
             association_index <
                 client->topology.view
                       .validator_endpoint_count;
             ++association_index) {
            const solana_delivery_validator_endpoint_t *
                association =
                    &client->topology
                           .validator_endpoints[
                               association_index
                           ];

            if (association->validator_index !=
                leader->validator_index) {
                continue;
            }

            candidates[output_index].leader_index =
                leader_index;
            candidates[output_index].validator_index =
                leader->validator_index;
            candidates[output_index].endpoint_index =
                association->endpoint_index;
            ++output_index;
        }
    }

    return SOLANA_DELIVERY_STATUS_OK;
}
