// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana_delivery_internal.h"

#include <stddef.h>
#include <stdint.h>

solana_delivery_status_t
solana_delivery_client_evaluate_submission_policy(
    const solana_delivery_client_t *client,
    const solana_delivery_submission_policy_t *policy
) {
    if (client == NULL || policy == NULL) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if (policy->max_topology_age_ns == 0U ||
        policy->target_limit == 0U) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if (!client->has_topology) {
        return SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE;
    }

    uint64_t now_monotonic_ns = 0U;
    solana_delivery_status_t status =
        client->monotonic_time_fn(
            client->monotonic_time_context,
            &now_monotonic_ns
        );

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        return status;
    }

    if (now_monotonic_ns <
        client->topology.received_monotonic_ns) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    const uint64_t topology_age_ns =
        now_monotonic_ns -
        client->topology.received_monotonic_ns;

    if (topology_age_ns >
        policy->max_topology_age_ns) {
        return SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE;
    }

    return SOLANA_DELIVERY_STATUS_OK;
}
