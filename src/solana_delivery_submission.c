// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana_delivery_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SUBMIT_OPTIONS_MIN_SIZE \
    (offsetof(solana_delivery_submit_options_t, reserved0) + \
     sizeof(((solana_delivery_submit_options_t *)0)->reserved0))

static bool pointer_is_aligned_for(
    const void *pointer,
    size_t alignment
) {
    return pointer != NULL &&
           alignment != 0U &&
           ((uintptr_t)pointer % alignment) == 0U;
}

static solana_delivery_status_t validate_submit_options(
    const solana_delivery_submit_options_t *options,
    solana_delivery_submission_policy_t *out_policy
) {
    if (options == NULL ||
        out_policy == NULL ||
        !pointer_is_aligned_for(
            options,
            _Alignof(solana_delivery_submit_options_t)
        )) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if ((size_t)options->struct_size <
        SUBMIT_OPTIONS_MIN_SIZE) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if (options->flags !=
        SOLANA_DELIVERY_SUBMIT_FLAGS_NONE) {
        return SOLANA_DELIVERY_STATUS_UNSUPPORTED;
    }

    if (options->reserved0 != 0U ||
        options->max_topology_age_ns == 0U ||
        options->target_limit == 0U) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    out_policy->max_topology_age_ns =
        options->max_topology_age_ns;
    out_policy->target_limit =
        (size_t)options->target_limit;

    return SOLANA_DELIVERY_STATUS_OK;
}

static solana_delivery_status_t allocate_array(
    solana_delivery_client_t *client,
    size_t count,
    size_t element_size,
    void **out_pointer
) {
    if (client == NULL ||
        out_pointer == NULL ||
        element_size == 0U) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    *out_pointer = NULL;

    if (count == 0U) {
        return SOLANA_DELIVERY_STATUS_OK;
    }

    if (count > SIZE_MAX / element_size) {
        return SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED;
    }

    void *pointer = client->allocator.calloc_fn(
        client->allocator.context,
        count,
        element_size
    );

    if (pointer == NULL) {
        return SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED;
    }

    *out_pointer = pointer;
    return SOLANA_DELIVERY_STATUS_OK;
}

static solana_delivery_status_t materialize_targets(
    const solana_delivery_client_t *client,
    const solana_delivery_route_target_t *routes,
    size_t route_count,
    solana_delivery_owned_request_target_t *targets
) {
    if (client == NULL ||
        routes == NULL ||
        targets == NULL ||
        route_count == 0U) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    for (size_t index = 0U;
         index < route_count;
         ++index) {
        const solana_delivery_route_target_t *route =
            &routes[index];

        if (route->leader_index >=
                client->topology.view.leader_count ||
            route->validator_index >=
                client->topology.view.validator_count ||
            route->endpoint_index >=
                client->topology.view.endpoint_count) {
            return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
        }

        targets[index].leader_index =
            route->leader_index;
        targets[index].validator_index =
            route->validator_index;
        targets[index].endpoint_index =
            route->endpoint_index;
        targets[index].validator_identity =
            client->topology
                  .validators[route->validator_index]
                  .identity;
        targets[index].endpoint =
            client->topology
                  .endpoints[route->endpoint_index];
    }

    return SOLANA_DELIVERY_STATUS_OK;
}

solana_delivery_status_t solana_delivery_client_submit(
    solana_delivery_client_t *client,
    const uint8_t *transaction_bytes,
    size_t transaction_length,
    const solana_delivery_submit_options_t *options,
    solana_delivery_request_id_t *out_request_id
) {
    if (!pointer_is_aligned_for(
            out_request_id,
            _Alignof(solana_delivery_request_id_t)
        )) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    *out_request_id =
        SOLANA_DELIVERY_REQUEST_ID_NONE;

    if (client == NULL ||
        transaction_bytes == NULL ||
        transaction_length == 0U) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    solana_delivery_submission_policy_t policy;
    solana_delivery_status_t status =
        validate_submit_options(
            options,
            &policy
        );

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        return status;
    }

    status = solana_delivery_client_evaluate_submission_policy(
        client,
        &policy
    );

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        return status;
    }

    const uint64_t routing_slot =
        client->topology.view.current_slot;

    size_t candidate_count = 0U;
    status = solana_delivery_client_resolve_slot(
        client,
        routing_slot,
        NULL,
        0U,
        &candidate_count
    );

    if (status !=
        SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED) {
        return status;
    }

    if (candidate_count == 0U) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    solana_delivery_topology_candidate_t *candidates = NULL;
    solana_delivery_route_target_t *routes = NULL;
    uint8_t *transaction_copy = NULL;
    solana_delivery_owned_request_target_t *owned_targets =
        NULL;
    solana_delivery_owned_request_t *request = NULL;

    status = allocate_array(
        client,
        candidate_count,
        sizeof(*candidates),
        (void **)&candidates
    );

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        goto cleanup;
    }

    size_t resolved_count = 0U;
    status = solana_delivery_client_resolve_slot(
        client,
        routing_slot,
        candidates,
        candidate_count,
        &resolved_count
    );

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        goto cleanup;
    }

    if (resolved_count != candidate_count) {
        status = SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
        goto cleanup;
    }

    size_t target_count = 0U;
    size_t unique_target_count = 0U;

    status = solana_delivery_plan_routes(
        candidates,
        candidate_count,
        policy.target_limit,
        NULL,
        0U,
        &target_count,
        &unique_target_count
    );

    if (status !=
        SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED) {
        goto cleanup;
    }

    if (target_count == 0U ||
        unique_target_count < target_count) {
        status = SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
        goto cleanup;
    }

    status = allocate_array(
        client,
        target_count,
        sizeof(*routes),
        (void **)&routes
    );

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        goto cleanup;
    }

    size_t planned_target_count = 0U;
    size_t planned_unique_target_count = 0U;

    status = solana_delivery_plan_routes(
        candidates,
        candidate_count,
        policy.target_limit,
        routes,
        target_count,
        &planned_target_count,
        &planned_unique_target_count
    );

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        goto cleanup;
    }

    if (planned_target_count != target_count ||
        planned_unique_target_count !=
            unique_target_count) {
        status = SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
        goto cleanup;
    }

    status = allocate_array(
        client,
        1U,
        transaction_length,
        (void **)&transaction_copy
    );

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        goto cleanup;
    }

    memcpy(
        transaction_copy,
        transaction_bytes,
        transaction_length
    );

    status = allocate_array(
        client,
        target_count,
        sizeof(*owned_targets),
        (void **)&owned_targets
    );

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        goto cleanup;
    }

    status = materialize_targets(
        client,
        routes,
        target_count,
        owned_targets
    );

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        goto cleanup;
    }

    status = allocate_array(
        client,
        1U,
        sizeof(*request),
        (void **)&request
    );

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        goto cleanup;
    }

    if (client->next_request_id ==
        SOLANA_DELIVERY_REQUEST_ID_NONE) {
        status =
            SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED;
        goto cleanup;
    }

    const solana_delivery_request_id_t request_id =
        client->next_request_id;

    request->request_id = request_id;
    request->topology_generation =
        client->topology.view.generation;
    request->routing_slot = routing_slot;
    request->transaction_bytes = transaction_copy;
    request->transaction_length = transaction_length;
    request->targets = owned_targets;
    request->target_count = target_count;
    request->state =
        SOLANA_DELIVERY_REQUEST_STATE_ACCEPTED;
    request->next_event_sequence = UINT64_C(2);
    request->next = client->request_head;

    if (!solana_delivery_event_queue_has_capacity(client)) {
        status =
            SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED;
        goto cleanup;
    }

    uint64_t accepted_time_ns = 0U;
    status = client->monotonic_time_fn(
        client->monotonic_time_context,
        &accepted_time_ns
    );

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        goto cleanup;
    }

    if (accepted_time_ns <
        client->topology.received_monotonic_ns) {
        status = SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
        goto cleanup;
    }

    solana_delivery_event_t accepted_event = {
        .struct_size =
            (uint32_t)sizeof(solana_delivery_event_t),
        .event_class =
            SOLANA_DELIVERY_EVENT_CLASS_REQUEST,
        .event_code =
            SOLANA_DELIVERY_REQUEST_EVENT_ACCEPTED,
        .diagnostic_code = 0,
        .request_id = request_id,
        .attempt_id =
            SOLANA_DELIVERY_ATTEMPT_ID_NONE,
        .request_sequence = UINT64_C(1),
        .monotonic_time_ns = accepted_time_ns,
        .reserved = {UINT64_C(0), UINT64_C(0)},
    };

    status = solana_delivery_event_queue_push(
        client,
        &accepted_event
    );

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        goto cleanup;
    }

    client->request_head = request;

    if (request_id == UINT64_MAX) {
        client->next_request_id =
            SOLANA_DELIVERY_REQUEST_ID_NONE;
    } else {
        client->next_request_id = request_id + UINT64_C(1);
    }

    *out_request_id = request_id;
    status = SOLANA_DELIVERY_STATUS_OK;

cleanup:
    client->allocator.free_fn(
        client->allocator.context,
        candidates
    );
    client->allocator.free_fn(
        client->allocator.context,
        routes
    );

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        client->allocator.free_fn(
            client->allocator.context,
            transaction_copy
        );
        client->allocator.free_fn(
            client->allocator.context,
            owned_targets
        );
        client->allocator.free_fn(
            client->allocator.context,
            request
        );
    }

    return status;
}
