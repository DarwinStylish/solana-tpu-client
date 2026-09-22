// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/delivery.h"
#include "solana_delivery_internal.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t calls;
    solana_delivery_status_t status;
    uint64_t value_ns;
} controlled_clock_state_t;

static void *test_calloc(
    void *context,
    size_t count,
    size_t size
) {
    (void)context;
    return calloc(count, size);
}

static void test_free(
    void *context,
    void *pointer
) {
    (void)context;
    free(pointer);
}

static solana_delivery_status_t controlled_clock(
    void *context,
    uint64_t *out_time_ns
) {
    controlled_clock_state_t *state = context;

    assert(state != NULL);
    ++state->calls;

    if (state->status !=
        SOLANA_DELIVERY_STATUS_OK) {
        return state->status;
    }

    if (out_time_ns == NULL) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    *out_time_ns = state->value_ns;
    return SOLANA_DELIVERY_STATUS_OK;
}

static solana_delivery_client_t *new_controlled_client(
    controlled_clock_state_t *clock_state
) {
    solana_delivery_allocator_t allocator = {
        .calloc_fn = test_calloc,
        .free_fn = test_free,
        .context = NULL,
    };

    solana_delivery_client_t *client = NULL;

    assert(
        solana_delivery_client_create_with_dependencies(
            &allocator,
            controlled_clock,
            clock_state,
            &client
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(client != NULL);
    return client;
}

static void install_empty_topology(
    solana_delivery_client_t *client,
    controlled_clock_state_t *clock_state,
    uint64_t generation,
    uint64_t current_slot,
    uint64_t receipt_time_ns
) {
    solana_delivery_topology_t topology;
    memset(&topology, 0, sizeof(topology));

    topology.struct_size =
        (uint32_t)sizeof(topology);
    topology.generation = generation;
    topology.current_slot = current_slot;

    clock_state->calls = 0U;
    clock_state->status =
        SOLANA_DELIVERY_STATUS_OK;
    clock_state->value_ns = receipt_time_ns;

    assert(
        solana_delivery_client_install_topology(
            client,
            &topology
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(clock_state->calls == 1U);
    assert(client->has_topology);
    assert(
        client->topology.received_monotonic_ns ==
        receipt_time_ns
    );
}

static solana_delivery_submission_policy_t valid_policy(void) {
    return (solana_delivery_submission_policy_t){
        .max_topology_age_ns = UINT64_C(100),
        .target_limit = 2U,
    };
}

static void test_argument_contract(void) {
    controlled_clock_state_t clock_state = {
        .calls = 0U,
        .status = SOLANA_DELIVERY_STATUS_OK,
        .value_ns = UINT64_C(1000),
    };

    solana_delivery_client_t *client =
        new_controlled_client(&clock_state);

    solana_delivery_submission_policy_t policy =
        valid_policy();

    assert(
        solana_delivery_client_evaluate_submission_policy(
            NULL,
            &policy
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    assert(
        solana_delivery_client_evaluate_submission_policy(
            client,
            NULL
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    policy.max_topology_age_ns = 0U;
    assert(
        solana_delivery_client_evaluate_submission_policy(
            client,
            &policy
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    policy = valid_policy();
    policy.target_limit = 0U;
    assert(
        solana_delivery_client_evaluate_submission_policy(
            client,
            &policy
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    assert(clock_state.calls == 0U);
    solana_delivery_client_destroy(client);
}

static void test_topology_unavailable_precedes_clock(void) {
    controlled_clock_state_t clock_state = {
        .calls = 0U,
        .status = SOLANA_DELIVERY_STATUS_OK,
        .value_ns = UINT64_C(1000),
    };

    solana_delivery_client_t *client =
        new_controlled_client(&clock_state);

    const solana_delivery_submission_policy_t policy =
        valid_policy();

    assert(
        solana_delivery_client_evaluate_submission_policy(
            client,
            &policy
        ) ==
        SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE
    );

    assert(clock_state.calls == 0U);
    solana_delivery_client_destroy(client);
}

static void test_freshness_boundaries(void) {
    controlled_clock_state_t clock_state = {0};
    solana_delivery_client_t *client =
        new_controlled_client(&clock_state);

    install_empty_topology(
        client,
        &clock_state,
        UINT64_C(7),
        UINT64_C(999999),
        UINT64_C(1000)
    );

    const solana_delivery_submission_policy_t policy =
        valid_policy();

    clock_state.calls = 0U;
    clock_state.value_ns = UINT64_C(1000);
    assert(
        solana_delivery_client_evaluate_submission_policy(
            client,
            &policy
        ) == SOLANA_DELIVERY_STATUS_OK
    );
    assert(clock_state.calls == 1U);

    clock_state.calls = 0U;
    clock_state.value_ns = UINT64_C(1099);
    assert(
        solana_delivery_client_evaluate_submission_policy(
            client,
            &policy
        ) == SOLANA_DELIVERY_STATUS_OK
    );
    assert(clock_state.calls == 1U);

    clock_state.calls = 0U;
    clock_state.value_ns = UINT64_C(1100);
    assert(
        solana_delivery_client_evaluate_submission_policy(
            client,
            &policy
        ) == SOLANA_DELIVERY_STATUS_OK
    );
    assert(clock_state.calls == 1U);

    clock_state.calls = 0U;
    clock_state.value_ns = UINT64_C(1101);
    assert(
        solana_delivery_client_evaluate_submission_policy(
            client,
            &policy
        ) == SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE
    );
    assert(clock_state.calls == 1U);

    assert(client->topology.view.generation ==
           UINT64_C(7));
    assert(client->topology.view.current_slot ==
           UINT64_C(999999));
    assert(client->topology.received_monotonic_ns ==
           UINT64_C(1000));

    solana_delivery_client_destroy(client);
}

static void test_backward_clock_is_internal_error(void) {
    controlled_clock_state_t clock_state = {0};
    solana_delivery_client_t *client =
        new_controlled_client(&clock_state);

    install_empty_topology(
        client,
        &clock_state,
        UINT64_C(1),
        UINT64_C(42),
        UINT64_C(500)
    );

    const solana_delivery_submission_policy_t policy =
        valid_policy();

    clock_state.calls = 0U;
    clock_state.value_ns = UINT64_C(499);

    assert(
        solana_delivery_client_evaluate_submission_policy(
            client,
            &policy
        ) == SOLANA_DELIVERY_STATUS_INTERNAL_ERROR
    );

    assert(clock_state.calls == 1U);
    assert(client->topology.received_monotonic_ns ==
           UINT64_C(500));

    solana_delivery_client_destroy(client);
}

static void test_clock_status_is_propagated(void) {
    controlled_clock_state_t clock_state = {0};
    solana_delivery_client_t *client =
        new_controlled_client(&clock_state);

    install_empty_topology(
        client,
        &clock_state,
        UINT64_C(2),
        UINT64_C(88),
        UINT64_C(700)
    );

    const solana_delivery_submission_policy_t policy =
        valid_policy();

    clock_state.calls = 0U;
    clock_state.status =
        SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED;
    clock_state.value_ns = UINT64_C(900);

    assert(
        solana_delivery_client_evaluate_submission_policy(
            client,
            &policy
        ) ==
        SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED
    );

    assert(clock_state.calls == 1U);
    assert(client->topology.view.generation ==
           UINT64_C(2));
    assert(client->topology.view.current_slot ==
           UINT64_C(88));
    assert(client->topology.received_monotonic_ns ==
           UINT64_C(700));

    solana_delivery_client_destroy(client);
}

static void test_generation_and_slot_do_not_define_age(void) {
    controlled_clock_state_t clock_state = {0};
    solana_delivery_client_t *client =
        new_controlled_client(&clock_state);

    install_empty_topology(
        client,
        &clock_state,
        UINT64_MAX,
        UINT64_MAX,
        UINT64_C(2000)
    );

    const solana_delivery_submission_policy_t policy = {
        .max_topology_age_ns = UINT64_C(10),
        .target_limit = 1U,
    };

    clock_state.calls = 0U;
    clock_state.value_ns = UINT64_C(2001);

    assert(
        solana_delivery_client_evaluate_submission_policy(
            client,
            &policy
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(clock_state.calls == 1U);
    solana_delivery_client_destroy(client);
}

int main(void) {
    test_argument_contract();
    test_topology_unavailable_precedes_clock();
    test_freshness_boundaries();
    test_backward_clock_is_internal_error();
    test_clock_status_is_propagated();
    test_generation_and_slot_do_not_define_age();
    return 0;
}
