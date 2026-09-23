// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/delivery.h"
#include "solana_delivery_internal.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    solana_delivery_validator_t validators[2];
    solana_delivery_endpoint_t endpoints[3];
    solana_delivery_validator_endpoint_t associations[4];
    solana_delivery_leader_t leaders[2];
    solana_delivery_topology_t topology;
} submission_fixture_t;

typedef struct {
    solana_delivery_submit_options_t base;
    uint64_t extension;
} extended_submit_options_t;

typedef struct {
    size_t allocation_calls;
    size_t free_calls;
    size_t fail_on_call;
} allocator_state_t;

typedef struct {
    size_t calls;
    size_t fail_on_call;
    solana_delivery_status_t status;
    uint64_t value_ns;
} clock_state_t;

static void *controlled_calloc(
    void *context,
    size_t count,
    size_t size
) {
    allocator_state_t *state = context;
    assert(state != NULL);

    ++state->allocation_calls;

    if (state->fail_on_call != 0U &&
        state->allocation_calls ==
            state->fail_on_call) {
        return NULL;
    }

    return calloc(count, size);
}

static void controlled_free(
    void *context,
    void *pointer
) {
    allocator_state_t *state = context;
    assert(state != NULL);

    if (pointer != NULL) {
        ++state->free_calls;
    }

    free(pointer);
}

static solana_delivery_status_t controlled_clock(
    void *context,
    uint64_t *out_time_ns
) {
    clock_state_t *state = context;
    assert(state != NULL);

    ++state->calls;

    if (state->status !=
            SOLANA_DELIVERY_STATUS_OK &&
        (state->fail_on_call == 0U ||
         state->calls == state->fail_on_call)) {
        return state->status;
    }

    if (out_time_ns == NULL) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    *out_time_ns = state->value_ns;
    return SOLANA_DELIVERY_STATUS_OK;
}

static void init_fixture(
    submission_fixture_t *fixture,
    uint64_t generation
) {
    memset(fixture, 0, sizeof(*fixture));

    for (uint32_t index = 0U;
         index < UINT32_C(2);
         ++index) {
        fixture->validators[index].struct_size =
            (uint32_t)sizeof(fixture->validators[index]);
        fixture->validators[index].identity.bytes[0] =
            (uint8_t)(UINT8_C(10) + (uint8_t)index);
    }

    for (uint32_t index = 0U;
         index < UINT32_C(3);
         ++index) {
        fixture->endpoints[index].struct_size =
            (uint32_t)sizeof(fixture->endpoints[index]);
        fixture->endpoints[index].address_family =
            SOLANA_DELIVERY_ADDRESS_IPV4;
        fixture->endpoints[index].transport =
            SOLANA_DELIVERY_TRANSPORT_QUIC;
        fixture->endpoints[index].role =
            SOLANA_DELIVERY_ENDPOINT_ROLE_TPU;
        fixture->endpoints[index].port =
            (uint16_t)(UINT16_C(8000) +
                       (uint16_t)index);
        fixture->endpoints[index].address[0] =
            UINT8_C(127);
        fixture->endpoints[index].address[3] =
            (uint8_t)(index + UINT32_C(1));
    }

    for (uint32_t index = 0U;
         index < UINT32_C(4);
         ++index) {
        fixture->associations[index].struct_size =
            (uint32_t)sizeof(
                fixture->associations[index]
            );
    }

    fixture->associations[0].validator_index =
        UINT32_C(0);
    fixture->associations[0].endpoint_index =
        UINT32_C(1);
    fixture->associations[1].validator_index =
        UINT32_C(1);
    fixture->associations[1].endpoint_index =
        UINT32_C(2);
    fixture->associations[2].validator_index =
        UINT32_C(1);
    fixture->associations[2].endpoint_index =
        UINT32_C(0);
    fixture->associations[3].validator_index =
        UINT32_C(0);
    fixture->associations[3].endpoint_index =
        UINT32_C(0);

    for (uint32_t index = 0U;
         index < UINT32_C(2);
         ++index) {
        fixture->leaders[index].struct_size =
            (uint32_t)sizeof(fixture->leaders[index]);
    }

    fixture->leaders[0].validator_index =
        UINT32_C(1);
    fixture->leaders[0].first_slot = UINT64_C(100);
    fixture->leaders[0].last_slot = UINT64_C(105);

    fixture->leaders[1].validator_index =
        UINT32_C(0);
    fixture->leaders[1].first_slot = UINT64_C(103);
    fixture->leaders[1].last_slot = UINT64_C(108);

    fixture->topology.struct_size =
        (uint32_t)sizeof(fixture->topology);
    fixture->topology.generation = generation;
    fixture->topology.current_slot = UINT64_C(103);

    fixture->topology.validators =
        fixture->validators;
    fixture->topology.validator_count = UINT32_C(2);
    fixture->topology.validator_stride =
        (uint32_t)sizeof(fixture->validators[0]);

    fixture->topology.endpoints =
        fixture->endpoints;
    fixture->topology.endpoint_count = UINT32_C(3);
    fixture->topology.endpoint_stride =
        (uint32_t)sizeof(fixture->endpoints[0]);

    fixture->topology.validator_endpoints =
        fixture->associations;
    fixture->topology.validator_endpoint_count =
        UINT32_C(4);
    fixture->topology.validator_endpoint_stride =
        (uint32_t)sizeof(fixture->associations[0]);

    fixture->topology.leaders = fixture->leaders;
    fixture->topology.leader_count = UINT32_C(2);
    fixture->topology.leader_stride =
        (uint32_t)sizeof(fixture->leaders[0]);
}

static solana_delivery_submit_options_t valid_options(void) {
    return (solana_delivery_submit_options_t){
        .struct_size =
            (uint32_t)sizeof(
                solana_delivery_submit_options_t
            ),
        .flags = SOLANA_DELIVERY_SUBMIT_FLAGS_NONE,
        .max_topology_age_ns = UINT64_C(100),
        .target_limit = UINT32_C(2),
        .reserved0 = 0U,
    };
}

static solana_delivery_client_t *new_client(
    allocator_state_t *allocator_state,
    clock_state_t *clock_state
) {
    solana_delivery_allocator_t allocator = {
        .calloc_fn = controlled_calloc,
        .free_fn = controlled_free,
        .context = allocator_state,
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
    assert(client->next_request_id == UINT64_C(1));
    assert(client->request_head == NULL);
    assert(client->event_queue.head == 0U);
    assert(client->event_queue.count == 0U);

    return client;
}

static void install_fixture(
    solana_delivery_client_t *client,
    submission_fixture_t *fixture,
    clock_state_t *clock_state,
    uint64_t receipt_time_ns
) {
    clock_state->calls = 0U;
    clock_state->status = SOLANA_DELIVERY_STATUS_OK;
    clock_state->value_ns = receipt_time_ns;

    assert(
        solana_delivery_client_install_topology(
            client,
            &fixture->topology
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(clock_state->calls == 1U);
}

static void assert_target(
    const solana_delivery_owned_request_target_t *target,
    uint32_t leader_index,
    uint32_t validator_index,
    uint32_t endpoint_index,
    uint8_t identity_first_byte,
    uint16_t port
) {
    assert(target->leader_index == leader_index);
    assert(target->validator_index == validator_index);
    assert(target->endpoint_index == endpoint_index);
    assert(
        target->validator_identity.bytes[0] ==
        identity_first_byte
    );
    assert(target->endpoint.port == port);
}

static void test_argument_and_option_contract(void) {
    allocator_state_t allocator_state = {0};
    clock_state_t clock_state = {
        .calls = 0U,
        .status = SOLANA_DELIVERY_STATUS_OK,
        .value_ns = UINT64_C(1000),
    };

    solana_delivery_client_t *client =
        new_client(&allocator_state, &clock_state);

    uint8_t transaction[1] = {UINT8_C(7)};
    solana_delivery_submit_options_t options =
        valid_options();
    solana_delivery_request_id_t request_id =
        UINT64_C(99);

    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            NULL
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    _Alignas(solana_delivery_request_id_t)
    uint8_t request_id_storage[
        sizeof(solana_delivery_request_id_t) + 1U
    ];

    solana_delivery_request_id_t *misaligned_request_id =
        (solana_delivery_request_id_t *)
            (request_id_storage + 1U);

    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            misaligned_request_id
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    request_id = UINT64_C(99);
    assert(
        solana_delivery_client_submit(
            NULL,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);

    request_id = UINT64_C(99);
    assert(
        solana_delivery_client_submit(
            client,
            NULL,
            sizeof(transaction),
            &options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);

    request_id = UINT64_C(99);
    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            0U,
            &options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);

    request_id = UINT64_C(99);
    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            NULL,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);

    _Alignas(solana_delivery_submit_options_t)
    uint8_t options_storage[
        sizeof(solana_delivery_submit_options_t) + 1U
    ];

    const solana_delivery_submit_options_t *misaligned_options =
        (const solana_delivery_submit_options_t *)
            (options_storage + 1U);

    request_id = UINT64_C(99);
    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            misaligned_options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);

    options = valid_options();
    options.struct_size =
        (uint32_t)(sizeof(options) - 1U);
    request_id = UINT64_C(99);
    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);

    options = valid_options();
    options.flags = UINT32_C(1);
    request_id = UINT64_C(99);
    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_UNSUPPORTED
    );
    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);

    options = valid_options();
    options.reserved0 = UINT32_C(1);
    request_id = UINT64_C(99);
    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);

    options = valid_options();
    options.max_topology_age_ns = 0U;
    request_id = UINT64_C(99);
    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);

    options = valid_options();
    options.target_limit = 0U;
    request_id = UINT64_C(99);
    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);

    assert(client->request_head == NULL);
    assert(client->next_request_id == UINT64_C(1));
    assert(clock_state.calls == 0U);

    solana_delivery_client_destroy(client);
}

static void test_unavailable_topology_precedes_clock(void) {
    allocator_state_t allocator_state = {0};
    clock_state_t clock_state = {
        .calls = 0U,
        .status = SOLANA_DELIVERY_STATUS_OK,
        .value_ns = UINT64_C(1000),
    };

    solana_delivery_client_t *client =
        new_client(&allocator_state, &clock_state);

    uint8_t transaction[1] = {UINT8_C(1)};
    solana_delivery_submit_options_t options =
        valid_options();
    solana_delivery_request_id_t request_id =
        UINT64_C(99);

    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) ==
        SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE
    );

    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);
    assert(clock_state.calls == 0U);
    assert(client->request_head == NULL);

    solana_delivery_client_destroy(client);
}

static void test_stale_and_clock_failure_are_atomic(void) {
    submission_fixture_t fixture;
    init_fixture(&fixture, UINT64_C(1));

    allocator_state_t allocator_state = {0};
    clock_state_t clock_state = {0};

    solana_delivery_client_t *client =
        new_client(&allocator_state, &clock_state);

    install_fixture(
        client,
        &fixture,
        &clock_state,
        UINT64_C(1000)
    );

    allocator_state.allocation_calls = 0U;
    allocator_state.free_calls = 0U;

    uint8_t transaction[2] = {
        UINT8_C(1),
        UINT8_C(2),
    };

    solana_delivery_submit_options_t options =
        valid_options();
    solana_delivery_request_id_t request_id =
        UINT64_C(99);

    clock_state.calls = 0U;
    clock_state.status = SOLANA_DELIVERY_STATUS_OK;
    clock_state.value_ns = UINT64_C(1101);

    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE
    );

    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);
    assert(clock_state.calls == 1U);
    assert(allocator_state.allocation_calls == 0U);
    assert(client->request_head == NULL);
    assert(client->next_request_id == UINT64_C(1));

    request_id = UINT64_C(99);
    clock_state.calls = 0U;
    clock_state.status =
        SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;

    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_INTERNAL_ERROR
    );

    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);
    assert(clock_state.calls == 1U);
    assert(allocator_state.allocation_calls == 0U);
    assert(client->request_head == NULL);

    solana_delivery_client_destroy(client);
}

static void test_fresh_topology_without_route_is_unavailable(void) {
    submission_fixture_t fixture;
    init_fixture(&fixture, UINT64_C(1));
    fixture.topology.current_slot = UINT64_C(999);

    allocator_state_t allocator_state = {0};
    clock_state_t clock_state = {0};

    solana_delivery_client_t *client =
        new_client(&allocator_state, &clock_state);

    install_fixture(
        client,
        &fixture,
        &clock_state,
        UINT64_C(1000)
    );

    allocator_state.allocation_calls = 0U;
    allocator_state.free_calls = 0U;
    clock_state.calls = 0U;
    clock_state.value_ns = UINT64_C(1050);

    uint8_t transaction[1] = {UINT8_C(1)};
    solana_delivery_submit_options_t options =
        valid_options();
    solana_delivery_request_id_t request_id =
        UINT64_C(99);

    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) ==
        SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE
    );

    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);
    assert(clock_state.calls == 1U);
    assert(allocator_state.allocation_calls == 0U);
    assert(client->request_head == NULL);
    assert(client->next_request_id == UINT64_C(1));

    solana_delivery_client_destroy(client);
}

static void test_success_owns_transaction_and_targets(void) {
    submission_fixture_t fixture;
    init_fixture(&fixture, UINT64_C(1));

    allocator_state_t allocator_state = {0};
    clock_state_t clock_state = {0};

    solana_delivery_client_t *client =
        new_client(&allocator_state, &clock_state);

    install_fixture(
        client,
        &fixture,
        &clock_state,
        UINT64_C(1000)
    );

    allocator_state.allocation_calls = 0U;
    allocator_state.free_calls = 0U;
    clock_state.calls = 0U;
    clock_state.value_ns = UINT64_C(1050);

    uint8_t transaction[] = {
        UINT8_C(1),
        UINT8_C(2),
        UINT8_C(3),
        UINT8_C(4),
    };

    extended_submit_options_t extended = {0};
    extended.base = valid_options();
    extended.base.struct_size =
        (uint32_t)sizeof(extended);
    extended.extension = UINT64_C(1234);

    solana_delivery_request_id_t request_id =
        SOLANA_DELIVERY_REQUEST_ID_NONE;

    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &extended.base,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(request_id == UINT64_C(1));
    assert(client->next_request_id == UINT64_C(2));
    assert(allocator_state.allocation_calls == 5U);
    assert(allocator_state.free_calls == 2U);
    assert(clock_state.calls == 2U);
    assert(client->event_queue.count == 1U);

    solana_delivery_owned_request_t *first =
        client->request_head;

    assert(first != NULL);
    assert(first->next == NULL);
    assert(first->request_id == UINT64_C(1));
    assert(first->topology_generation == UINT64_C(1));
    assert(first->routing_slot == UINT64_C(103));
    assert(
        first->state ==
        SOLANA_DELIVERY_REQUEST_STATE_ACCEPTED
    );
    assert(first->next_event_sequence == UINT64_C(2));
    assert(first->transaction_length ==
           sizeof(transaction));
    assert(first->transaction_bytes != transaction);
    assert(memcmp(
               first->transaction_bytes,
               transaction,
               sizeof(transaction)
           ) == 0);
    assert(first->target_count == 2U);

    assert_target(
        &first->targets[0],
        UINT32_C(0),
        UINT32_C(1),
        UINT32_C(2),
        UINT8_C(11),
        UINT16_C(8002)
    );
    assert_target(
        &first->targets[1],
        UINT32_C(0),
        UINT32_C(1),
        UINT32_C(0),
        UINT8_C(11),
        UINT16_C(8000)
    );

    solana_delivery_event_t accepted_event = {0};
    size_t accepted_event_count = 0U;

    assert(
        solana_delivery_client_poll_events(
            client,
            &accepted_event,
            1U,
            (uint32_t)sizeof(accepted_event),
            &accepted_event_count
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(accepted_event_count == 1U);
    assert(
        accepted_event.struct_size ==
        (uint32_t)sizeof(solana_delivery_event_t)
    );
    assert(
        accepted_event.event_class ==
        SOLANA_DELIVERY_EVENT_CLASS_REQUEST
    );
    assert(
        accepted_event.event_code ==
        SOLANA_DELIVERY_REQUEST_EVENT_ACCEPTED
    );
    assert(accepted_event.diagnostic_code == 0);
    assert(accepted_event.request_id == UINT64_C(1));
    assert(
        accepted_event.attempt_id ==
        SOLANA_DELIVERY_ATTEMPT_ID_NONE
    );
    assert(
        accepted_event.request_sequence ==
        UINT64_C(1)
    );
    assert(
        accepted_event.monotonic_time_ns ==
        UINT64_C(1050)
    );
    assert(accepted_event.reserved[0] == UINT64_C(0));
    assert(accepted_event.reserved[1] == UINT64_C(0));
    assert(client->event_queue.count == 0U);

    transaction[0] = UINT8_C(99);
    assert(first->transaction_bytes[0] == UINT8_C(1));

    submission_fixture_t replacement;
    init_fixture(&replacement, UINT64_C(2));

    replacement.validators[0].identity.bytes[0] =
        UINT8_C(50);
    replacement.validators[1].identity.bytes[0] =
        UINT8_C(51);

    replacement.endpoints[0].port = UINT16_C(9000);
    replacement.endpoints[1].port = UINT16_C(9001);
    replacement.endpoints[2].port = UINT16_C(9002);

    clock_state.value_ns = UINT64_C(1060);
    assert(
        solana_delivery_client_install_topology(
            client,
            &replacement.topology
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert_target(
        &first->targets[0],
        UINT32_C(0),
        UINT32_C(1),
        UINT32_C(2),
        UINT8_C(11),
        UINT16_C(8002)
    );
    assert_target(
        &first->targets[1],
        UINT32_C(0),
        UINT32_C(1),
        UINT32_C(0),
        UINT8_C(11),
        UINT16_C(8000)
    );

    uint8_t second_transaction[] = {
        UINT8_C(8),
        UINT8_C(9),
    };

    solana_delivery_submit_options_t options =
        valid_options();
    solana_delivery_request_id_t second_id =
        SOLANA_DELIVERY_REQUEST_ID_NONE;

    clock_state.value_ns = UINT64_C(1070);

    assert(
        solana_delivery_client_submit(
            client,
            second_transaction,
            sizeof(second_transaction),
            &options,
            &second_id
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(second_id == UINT64_C(2));
    assert(client->next_request_id == UINT64_C(3));
    assert(client->request_head != NULL);
    assert(client->request_head->next == first);
    assert(
        client->request_head->state ==
        SOLANA_DELIVERY_REQUEST_STATE_ACCEPTED
    );
    assert(
        client->request_head->next_event_sequence ==
        UINT64_C(2)
    );
    assert(client->event_queue.count == 1U);

    assert_target(
        &client->request_head->targets[0],
        UINT32_C(0),
        UINT32_C(1),
        UINT32_C(2),
        UINT8_C(51),
        UINT16_C(9002)
    );

    solana_delivery_client_destroy(client);
}

static void test_request_id_exhaustion(void) {
    submission_fixture_t fixture;
    init_fixture(&fixture, UINT64_C(1));

    allocator_state_t allocator_state = {0};
    clock_state_t clock_state = {0};

    solana_delivery_client_t *client =
        new_client(&allocator_state, &clock_state);

    install_fixture(
        client,
        &fixture,
        &clock_state,
        UINT64_C(1000)
    );

    client->next_request_id = UINT64_MAX;

    uint8_t transaction[1] = {UINT8_C(1)};
    solana_delivery_submit_options_t options =
        valid_options();
    solana_delivery_request_id_t request_id =
        SOLANA_DELIVERY_REQUEST_ID_NONE;

    clock_state.value_ns = UINT64_C(1050);

    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(request_id == UINT64_MAX);
    assert(
        client->next_request_id ==
        SOLANA_DELIVERY_REQUEST_ID_NONE
    );

    solana_delivery_owned_request_t *accepted =
        client->request_head;
    assert(accepted != NULL);

    allocator_state.allocation_calls = 0U;
    allocator_state.free_calls = 0U;
    request_id = UINT64_C(99);
    clock_state.value_ns = UINT64_C(1060);

    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) ==
        SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED
    );

    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);
    assert(allocator_state.allocation_calls == 5U);
    assert(allocator_state.free_calls == 5U);
    assert(client->request_head == accepted);

    solana_delivery_client_destroy(client);
}

static void test_allocation_failures_are_transactional(void) {
    submission_fixture_t fixture;
    init_fixture(&fixture, UINT64_C(1));

    allocator_state_t allocator_state = {0};
    clock_state_t clock_state = {0};

    solana_delivery_client_t *client =
        new_client(&allocator_state, &clock_state);

    install_fixture(
        client,
        &fixture,
        &clock_state,
        UINT64_C(1000)
    );

    uint8_t transaction[] = {
        UINT8_C(1),
        UINT8_C(2),
        UINT8_C(3),
    };

    solana_delivery_submit_options_t options =
        valid_options();

    for (size_t fail_on_call = 1U;
         fail_on_call <= 5U;
         ++fail_on_call) {
        allocator_state.allocation_calls = 0U;
        allocator_state.free_calls = 0U;
        allocator_state.fail_on_call = fail_on_call;

        clock_state.calls = 0U;
        clock_state.status = SOLANA_DELIVERY_STATUS_OK;
        clock_state.value_ns = UINT64_C(1050);

        solana_delivery_request_id_t request_id =
            UINT64_C(99);

        assert(
            solana_delivery_client_submit(
                client,
                transaction,
                sizeof(transaction),
                &options,
                &request_id
            ) ==
            SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED
        );

        assert(request_id ==
               SOLANA_DELIVERY_REQUEST_ID_NONE);
        assert(
            allocator_state.allocation_calls ==
            fail_on_call
        );
        assert(
            allocator_state.free_calls ==
            fail_on_call - 1U
        );
        assert(client->request_head == NULL);
        assert(client->next_request_id == UINT64_C(1));
    }

    allocator_state.allocation_calls = 0U;
    allocator_state.free_calls = 0U;
    allocator_state.fail_on_call = 0U;
    clock_state.value_ns = UINT64_C(1050);

    solana_delivery_request_id_t request_id =
        SOLANA_DELIVERY_REQUEST_ID_NONE;

    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(request_id == UINT64_C(1));
    assert(allocator_state.allocation_calls == 5U);
    assert(allocator_state.free_calls == 2U);
    assert(client->request_head != NULL);

    solana_delivery_client_destroy(client);

    assert(allocator_state.free_calls == 10U);
}


static void test_event_timestamp_failure_is_transactional(void) {
    submission_fixture_t fixture;
    init_fixture(&fixture, UINT64_C(1));

    allocator_state_t allocator_state = {0};
    clock_state_t clock_state = {0};

    solana_delivery_client_t *client =
        new_client(&allocator_state, &clock_state);

    install_fixture(
        client,
        &fixture,
        &clock_state,
        UINT64_C(1000)
    );

    allocator_state.allocation_calls = 0U;
    allocator_state.free_calls = 0U;

    clock_state.calls = 0U;
    clock_state.fail_on_call = 2U;
    clock_state.status =
        SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    clock_state.value_ns = UINT64_C(1050);

    const uint8_t transaction[] = {
        UINT8_C(1),
        UINT8_C(2),
    };

    solana_delivery_submit_options_t options =
        valid_options();

    solana_delivery_request_id_t request_id =
        UINT64_C(99);

    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_INTERNAL_ERROR
    );

    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);
    assert(clock_state.calls == 2U);
    assert(allocator_state.allocation_calls == 5U);
    assert(allocator_state.free_calls == 5U);
    assert(client->request_head == NULL);
    assert(client->next_request_id == UINT64_C(1));
    assert(client->event_queue.count == 0U);

    clock_state.fail_on_call = 0U;
    clock_state.status = SOLANA_DELIVERY_STATUS_OK;

    solana_delivery_client_destroy(client);
}

static void test_event_backpressure_is_transactional(void) {
    submission_fixture_t fixture;
    init_fixture(&fixture, UINT64_C(1));

    allocator_state_t allocator_state = {0};
    clock_state_t clock_state = {0};

    solana_delivery_client_t *client =
        new_client(&allocator_state, &clock_state);

    install_fixture(
        client,
        &fixture,
        &clock_state,
        UINT64_C(1000)
    );

    for (uint64_t index = UINT64_C(0);
         index <
             (uint64_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY;
         ++index) {
        const solana_delivery_event_t event = {
            .struct_size =
                (uint32_t)sizeof(
                    solana_delivery_event_t
                ),
            .event_class =
                SOLANA_DELIVERY_EVENT_CLASS_REQUEST,
            .event_code =
                SOLANA_DELIVERY_REQUEST_EVENT_ACCEPTED,
            .diagnostic_code = 0,
            .request_id = index + UINT64_C(100),
            .attempt_id =
                SOLANA_DELIVERY_ATTEMPT_ID_NONE,
            .request_sequence = UINT64_C(1),
            .monotonic_time_ns = index,
            .reserved = {
                UINT64_C(0),
                UINT64_C(0),
            },
        };

        assert(
            solana_delivery_event_queue_push(
                client,
                &event
            ) == SOLANA_DELIVERY_STATUS_OK
        );
    }

    assert(
        client->event_queue.count ==
        (size_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY
    );

    allocator_state.allocation_calls = 0U;
    allocator_state.free_calls = 0U;

    clock_state.calls = 0U;
    clock_state.status = SOLANA_DELIVERY_STATUS_OK;
    clock_state.value_ns = UINT64_C(1050);

    const uint8_t transaction[] = {
        UINT8_C(7),
        UINT8_C(8),
    };

    solana_delivery_submit_options_t options =
        valid_options();

    solana_delivery_request_id_t request_id =
        UINT64_C(99);

    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) ==
        SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED
    );

    assert(request_id == SOLANA_DELIVERY_REQUEST_ID_NONE);
    assert(clock_state.calls == 1U);
    assert(allocator_state.allocation_calls == 5U);
    assert(allocator_state.free_calls == 5U);
    assert(client->request_head == NULL);
    assert(client->next_request_id == UINT64_C(1));
    assert(
        client->event_queue.count ==
        (size_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY
    );

    solana_delivery_event_t drained_event = {0};
    size_t drained_count = 0U;

    assert(
        solana_delivery_client_poll_events(
            client,
            &drained_event,
            1U,
            (uint32_t)sizeof(drained_event),
            &drained_count
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(drained_count == 1U);
    assert(
        client->event_queue.count ==
        (size_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY -
            1U
    );

    allocator_state.allocation_calls = 0U;
    allocator_state.free_calls = 0U;

    clock_state.calls = 0U;
    clock_state.fail_on_call = 0U;
    clock_state.status = SOLANA_DELIVERY_STATUS_OK;
    clock_state.value_ns = UINT64_C(1050);

    request_id = SOLANA_DELIVERY_REQUEST_ID_NONE;

    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(request_id == UINT64_C(1));
    assert(clock_state.calls == 2U);
    assert(allocator_state.allocation_calls == 5U);
    assert(allocator_state.free_calls == 2U);
    assert(client->request_head != NULL);
    assert(client->next_request_id == UINT64_C(2));
    assert(
        client->event_queue.count ==
        (size_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY
    );

    solana_delivery_client_destroy(client);
}

int main(void) {
    test_argument_and_option_contract();
    test_unavailable_topology_precedes_clock();
    test_stale_and_clock_failure_are_atomic();
    test_fresh_topology_without_route_is_unavailable();
    test_success_owns_transaction_and_targets();
    test_request_id_exhaustion();
    test_allocation_failures_are_transactional();
    test_event_timestamp_failure_is_transactional();
    test_event_backpressure_is_transactional();
    return 0;
}
