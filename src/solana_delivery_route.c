// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana_delivery_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool candidate_has_same_target(
    const solana_delivery_topology_candidate_t *left,
    const solana_delivery_topology_candidate_t *right
) {
    return left->validator_index == right->validator_index &&
           left->endpoint_index == right->endpoint_index;
}

static bool candidate_is_first_occurrence(
    const solana_delivery_topology_candidate_t *candidates,
    size_t index
) {
    for (size_t prior = 0U; prior < index; ++prior) {
        if (candidate_has_same_target(
                &candidates[prior],
                &candidates[index]
            )) {
            return false;
        }
    }

    return true;
}

static size_t count_unique_targets(
    const solana_delivery_topology_candidate_t *candidates,
    size_t candidate_count
) {
    size_t unique_count = 0U;

    for (size_t index = 0U;
         index < candidate_count;
         ++index) {
        if (candidate_is_first_occurrence(
                candidates,
                index
            )) {
            ++unique_count;
        }
    }

    return unique_count;
}

solana_delivery_status_t
solana_delivery_plan_routes(
    const solana_delivery_topology_candidate_t *candidates,
    size_t candidate_count,
    size_t target_limit,
    solana_delivery_route_target_t *targets,
    size_t capacity,
    size_t *out_target_count,
    size_t *out_unique_target_count
) {
    if (out_target_count == NULL ||
        out_unique_target_count == NULL) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    *out_target_count = 0U;
    *out_unique_target_count = 0U;

    if (target_limit == 0U ||
        (candidate_count != 0U && candidates == NULL) ||
        (capacity != 0U && targets == NULL)) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if (candidate_count == 0U) {
        return SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE;
    }

    const size_t unique_count =
        count_unique_targets(candidates, candidate_count);

    const size_t selected_count =
        unique_count < target_limit
            ? unique_count
            : target_limit;

    *out_target_count = selected_count;
    *out_unique_target_count = unique_count;

    if (capacity < selected_count) {
        return SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED;
    }

    size_t output_index = 0U;

    for (size_t index = 0U;
         index < candidate_count &&
             output_index < selected_count;
         ++index) {
        if (!candidate_is_first_occurrence(
                candidates,
                index
            )) {
            continue;
        }

        targets[output_index].leader_index =
            candidates[index].leader_index;
        targets[output_index].validator_index =
            candidates[index].validator_index;
        targets[output_index].endpoint_index =
            candidates[index].endpoint_index;
        ++output_index;
    }

    return SOLANA_DELIVERY_STATUS_OK;
}
