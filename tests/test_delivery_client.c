// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/delivery.h"
#include "solana_delivery_internal.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    solana_delivery_validator_t validator;
    solana_delivery_endpoint_t endpoint;
    solana_delivery_validator_endpoint_t association;
    solana_delivery_leader_t leader;
    solana_delivery_topology_t topology;
} fixture_t;

typedef struct {
    solana_delivery_validator_t base;
    uint64_t extension;
} extended_validator_t;

typedef struct {
    size_t allocation_calls;
    size_t free_calls;
    size_t fail_on_call;
} failing_allocator_state_t;

typedef struct {
    size_t calls;
    bool fail;
    uint64_t value_ns;
} controlled_clock_state_t;

static void *failing_calloc(
    void *context,
    size_t count,
    size_t size
) {
    failing_allocator_state_t *state = context;

    assert(state != NULL);

    ++state->allocation_calls;

    if (state->fail_on_call != 0U &&
        state->allocation_calls == state->fail_on_call) {
        return NULL;
    }

    return calloc(count, size);
}

static void failing_free(
    void *context,
    void *pointer
) {
    failing_allocator_state_t *state = context;

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
    controlled_clock_state_t *state = context;

    assert(state != NULL);
    ++state->calls;

    if (state->fail) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    if (out_time_ns == NULL) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    *out_time_ns = state->value_ns;
    return SOLANA_DELIVERY_STATUS_OK;
}

static solana_delivery_client_t *
new_client_with_allocator(
    failing_allocator_state_t *state
) {
    solana_delivery_allocator_t allocator = {
        .calloc_fn = failing_calloc,
        .free_fn = failing_free,
        .context = state,
    };

    solana_delivery_client_t *client = NULL;

    assert(
        solana_delivery_client_create_with_allocator(
            &allocator,
            &client
        ) == SOLANA_DELIVERY_STATUS_OK
    );
    assert(client != NULL);

    return client;
}

static void init_fixture(
    fixture_t *fixture,
    uint64_t generation
) {
    memset(fixture, 0, sizeof(*fixture));

    fixture->validator.struct_size =
        (uint32_t)sizeof(fixture->validator);
    fixture->validator.identity.bytes[0] = UINT8_C(7);

    fixture->endpoint.struct_size =
        (uint32_t)sizeof(fixture->endpoint);
    fixture->endpoint.address_family =
        SOLANA_DELIVERY_ADDRESS_IPV4;
    fixture->endpoint.transport =
        SOLANA_DELIVERY_TRANSPORT_QUIC;
    fixture->endpoint.role =
        SOLANA_DELIVERY_ENDPOINT_ROLE_TPU;
    fixture->endpoint.port = UINT16_C(8003);
    fixture->endpoint.address[0] = UINT8_C(127);
    fixture->endpoint.address[3] = UINT8_C(1);

    fixture->association.struct_size =
        (uint32_t)sizeof(fixture->association);

    fixture->leader.struct_size =
        (uint32_t)sizeof(fixture->leader);
    fixture->leader.first_slot = UINT64_C(100);
    fixture->leader.last_slot = UINT64_C(104);

    fixture->topology.struct_size =
        (uint32_t)sizeof(fixture->topology);
    fixture->topology.generation = generation;
    fixture->topology.current_slot = UINT64_C(100);

    fixture->topology.validators = &fixture->validator;
    fixture->topology.validator_count = UINT32_C(1);
    fixture->topology.validator_stride =
        (uint32_t)sizeof(fixture->validator);

    fixture->topology.endpoints = &fixture->endpoint;
    fixture->topology.endpoint_count = UINT32_C(1);
    fixture->topology.endpoint_stride =
        (uint32_t)sizeof(fixture->endpoint);

    fixture->topology.validator_endpoints =
        &fixture->association;
    fixture->topology.validator_endpoint_count =
        UINT32_C(1);
    fixture->topology.validator_endpoint_stride =
        (uint32_t)sizeof(fixture->association);

    fixture->topology.leaders = &fixture->leader;
    fixture->topology.leader_count = UINT32_C(1);
    fixture->topology.leader_stride =
        (uint32_t)sizeof(fixture->leader);
}

static solana_delivery_client_t *new_client(void) {
    solana_delivery_client_t *client = NULL;
    assert(solana_delivery_client_create(&client) ==
           SOLANA_DELIVERY_STATUS_OK);
    assert(client != NULL);
    assert(!client->has_topology);
    return client;
}

static void test_client_lifecycle(void) {
    assert(solana_delivery_client_create(NULL) ==
           SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    solana_delivery_client_t *client = new_client();
    solana_delivery_client_destroy(client);
    solana_delivery_client_destroy(NULL);
}

static void test_deep_copy_and_normalization(void) {
    fixture_t fixture;
    init_fixture(&fixture, UINT64_C(5));

    solana_delivery_client_t *client = new_client();

    assert(solana_delivery_client_install_topology(
               client,
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_OK);

    assert(client->has_topology);
    assert(client->topology.view.generation ==
           UINT64_C(5));
    assert(client->topology.view.current_slot ==
           UINT64_C(100));
    assert(client->topology.validators !=
           &fixture.validator);
    assert(client->topology.endpoints !=
           &fixture.endpoint);
    assert(client->topology.validator_endpoints !=
           &fixture.association);
    assert(client->topology.leaders !=
           &fixture.leader);

    assert(client->topology.view.validator_stride ==
           sizeof(solana_delivery_validator_t));
    assert(client->topology.view.endpoint_stride ==
           sizeof(solana_delivery_endpoint_t));
    assert(
        client->topology.view.validator_endpoint_stride ==
        sizeof(solana_delivery_validator_endpoint_t)
    );
    assert(client->topology.view.leader_stride ==
           sizeof(solana_delivery_leader_t));

    assert(client->topology.validators[0].identity.bytes[0] ==
           UINT8_C(7));
    assert(client->topology.endpoints[0].port ==
           UINT16_C(8003));

    fixture.validator.identity.bytes[0] = UINT8_C(99);
    fixture.endpoint.port = UINT16_C(9000);

    assert(client->topology.validators[0].identity.bytes[0] ==
           UINT8_C(7));
    assert(client->topology.endpoints[0].port ==
           UINT16_C(8003));

    solana_delivery_client_destroy(client);
}

static void test_generation_ordering(void) {
    fixture_t fixture;
    solana_delivery_client_t *client = new_client();

    init_fixture(&fixture, UINT64_C(0));
    assert(solana_delivery_client_install_topology(
               client,
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_OK);

    assert(solana_delivery_client_install_topology(
               client,
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE);

    init_fixture(&fixture, UINT64_C(10));
    assert(solana_delivery_client_install_topology(
               client,
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_OK);

    init_fixture(&fixture, UINT64_C(9));
    assert(solana_delivery_client_install_topology(
               client,
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE);

    init_fixture(&fixture, UINT64_C(10));
    assert(solana_delivery_client_install_topology(
               client,
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE);

    init_fixture(&fixture, UINT64_C(11));
    assert(solana_delivery_client_install_topology(
               client,
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_OK);

    assert(client->topology.view.generation ==
           UINT64_C(11));

    solana_delivery_client_destroy(client);
}

static void test_invalid_replacement_is_transactional(void) {
    fixture_t first;
    fixture_t invalid;
    init_fixture(&first, UINT64_C(20));
    init_fixture(&invalid, UINT64_C(21));

    solana_delivery_client_t *client = new_client();

    assert(solana_delivery_client_install_topology(
               client,
               &first.topology
           ) == SOLANA_DELIVERY_STATUS_OK);

    solana_delivery_validator_t *old_validators =
        client->topology.validators;
    solana_delivery_endpoint_t *old_endpoints =
        client->topology.endpoints;
    uint64_t old_time =
        client->topology.received_monotonic_ns;

    invalid.endpoint.port = UINT16_C(0);

    assert(solana_delivery_client_install_topology(
               client,
               &invalid.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    assert(client->topology.view.generation ==
           UINT64_C(20));
    assert(client->topology.validators == old_validators);
    assert(client->topology.endpoints == old_endpoints);
    assert(client->topology.received_monotonic_ns ==
           old_time);

    solana_delivery_client_destroy(client);
}

static void test_extended_input_is_normalized(void) {
    extended_validator_t validators[2];
    memset(validators, 0, sizeof(validators));

    for (size_t i = 0U; i < 2U; ++i) {
        validators[i].base.struct_size =
            (uint32_t)sizeof(validators[i]);
        validators[i].base.identity.bytes[0] =
            (uint8_t)(i + 1U);
        validators[i].extension =
            UINT64_C(0x1122334455667788);
    }

    solana_delivery_topology_t topology;
    memset(&topology, 0, sizeof(topology));
    topology.struct_size = (uint32_t)sizeof(topology);
    topology.generation = UINT64_C(1);
    topology.validators =
        (const solana_delivery_validator_t *)validators;
    topology.validator_count = UINT32_C(2);
    topology.validator_stride =
        (uint32_t)sizeof(validators[0]);

    solana_delivery_client_t *client = new_client();

    assert(solana_delivery_client_install_topology(
               client,
               &topology
           ) == SOLANA_DELIVERY_STATUS_OK);

    assert(client->topology.view.validator_stride ==
           sizeof(solana_delivery_validator_t));
    assert(client->topology.validators[0].struct_size ==
           sizeof(solana_delivery_validator_t));
    assert(client->topology.validators[1].struct_size ==
           sizeof(solana_delivery_validator_t));
    assert(client->topology.validators[0].identity.bytes[0] ==
           UINT8_C(1));
    assert(client->topology.validators[1].identity.bytes[0] ==
           UINT8_C(2));

    validators[0].base.identity.bytes[0] = UINT8_C(55);
    assert(client->topology.validators[0].identity.bytes[0] ==
           UINT8_C(1));

    solana_delivery_client_destroy(client);
}

static void test_empty_snapshot_and_receipt_time(void) {
    solana_delivery_topology_t first;
    solana_delivery_topology_t second;

    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));

    first.struct_size = (uint32_t)sizeof(first);
    first.generation = UINT64_C(1);

    second.struct_size = (uint32_t)sizeof(second);
    second.generation = UINT64_C(2);

    solana_delivery_client_t *client = new_client();

    assert(solana_delivery_client_install_topology(
               client,
               &first
           ) == SOLANA_DELIVERY_STATUS_OK);

    uint64_t first_time =
        client->topology.received_monotonic_ns;

    assert(client->topology.view.validators == NULL);
    assert(client->topology.view.validator_count == 0U);

    assert(solana_delivery_client_install_topology(
               client,
               &second
           ) == SOLANA_DELIVERY_STATUS_OK);

    assert(client->topology.received_monotonic_ns >=
           first_time);

    solana_delivery_client_destroy(client);
}

static void test_null_install_arguments(void) {
    fixture_t fixture;
    init_fixture(&fixture, UINT64_C(1));

    assert(solana_delivery_client_install_topology(
               NULL,
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    solana_delivery_client_t *client = new_client();

    assert(solana_delivery_client_install_topology(
               client,
               NULL
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    solana_delivery_client_destroy(client);
}

static void test_allocation_failure_is_transactional(void) {
    fixture_t installed;
    fixture_t replacement;

    init_fixture(&installed, UINT64_C(40));
    init_fixture(&replacement, UINT64_C(41));

    replacement.topology.current_slot = UINT64_C(200);
    replacement.validator.identity.bytes[0] = UINT8_C(9);
    replacement.endpoint.port = UINT16_C(9001);
    replacement.leader.first_slot = UINT64_C(200);
    replacement.leader.last_slot = UINT64_C(204);

    failing_allocator_state_t state = {0};

    solana_delivery_client_t *client =
        new_client_with_allocator(&state);

    state.allocation_calls = 0U;
    state.fail_on_call = 0U;

    assert(solana_delivery_client_install_topology(
               client,
               &installed.topology
           ) == SOLANA_DELIVERY_STATUS_OK);

    solana_delivery_validator_t *old_validators =
        client->topology.validators;
    solana_delivery_endpoint_t *old_endpoints =
        client->topology.endpoints;
    solana_delivery_validator_endpoint_t *old_associations =
        client->topology.validator_endpoints;
    solana_delivery_leader_t *old_leaders =
        client->topology.leaders;

    const uint64_t old_generation =
        client->topology.view.generation;
    const uint64_t old_slot =
        client->topology.view.current_slot;
    const uint64_t old_receipt_time =
        client->topology.received_monotonic_ns;
    const uint8_t old_identity =
        client->topology.validators[0].identity.bytes[0];
    const uint16_t old_port =
        client->topology.endpoints[0].port;
    const uint64_t old_first_slot =
        client->topology.leaders[0].first_slot;
    const uint64_t old_last_slot =
        client->topology.leaders[0].last_slot;

    for (size_t fail_on_call = 1U;
         fail_on_call <= 4U;
         ++fail_on_call) {
        state.allocation_calls = 0U;
        state.fail_on_call = fail_on_call;

        assert(solana_delivery_client_install_topology(
                   client,
                   &replacement.topology
               ) ==
               SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED);

        assert(state.allocation_calls == fail_on_call);

        assert(client->has_topology);
        assert(client->topology.view.generation ==
               old_generation);
        assert(client->topology.view.current_slot ==
               old_slot);
        assert(client->topology.received_monotonic_ns ==
               old_receipt_time);

        assert(client->topology.validators ==
               old_validators);
        assert(client->topology.endpoints ==
               old_endpoints);
        assert(client->topology.validator_endpoints ==
               old_associations);
        assert(client->topology.leaders ==
               old_leaders);

        assert(
            client->topology.validators[0]
                    .identity.bytes[0] ==
            old_identity
        );
        assert(client->topology.endpoints[0].port ==
               old_port);
        assert(client->topology.leaders[0].first_slot ==
               old_first_slot);
        assert(client->topology.leaders[0].last_slot ==
               old_last_slot);
    }

    state.allocation_calls = 0U;
    state.fail_on_call = 0U;

    assert(solana_delivery_client_install_topology(
               client,
               &replacement.topology
           ) == SOLANA_DELIVERY_STATUS_OK);

    assert(state.allocation_calls == 4U);
    assert(client->topology.view.generation ==
           UINT64_C(41));
    assert(client->topology.view.current_slot ==
           UINT64_C(200));
    assert(
        client->topology.validators[0]
                .identity.bytes[0] ==
        UINT8_C(9)
    );
    assert(client->topology.endpoints[0].port ==
           UINT16_C(9001));
    assert(client->topology.leaders[0].first_slot ==
           UINT64_C(200));
    assert(client->topology.leaders[0].last_slot ==
           UINT64_C(204));

    assert(client->topology.validators !=
           old_validators);
    assert(client->topology.endpoints !=
           old_endpoints);
    assert(client->topology.validator_endpoints !=
           old_associations);
    assert(client->topology.leaders !=
           old_leaders);

    solana_delivery_client_destroy(client);
}

static void test_clock_failure_is_transactional(void) {
    fixture_t installed;
    fixture_t replacement;

    init_fixture(&installed, UINT64_C(50));
    init_fixture(&replacement, UINT64_C(51));

    replacement.topology.current_slot = UINT64_C(300);
    replacement.validator.identity.bytes[0] = UINT8_C(11);
    replacement.endpoint.port = UINT16_C(9002);
    replacement.leader.first_slot = UINT64_C(300);
    replacement.leader.last_slot = UINT64_C(304);

    failing_allocator_state_t allocator_state = {0};
    controlled_clock_state_t clock_state = {
        .calls = 0U,
        .fail = false,
        .value_ns = UINT64_C(123456789),
    };

    solana_delivery_allocator_t allocator = {
        .calloc_fn = failing_calloc,
        .free_fn = failing_free,
        .context = &allocator_state,
    };

    solana_delivery_client_t *client = NULL;

    assert(
        solana_delivery_client_create_with_dependencies(
            &allocator,
            controlled_clock,
            &clock_state,
            &client
        ) == SOLANA_DELIVERY_STATUS_OK
    );
    assert(client != NULL);

    allocator_state.allocation_calls = 0U;
    allocator_state.free_calls = 0U;

    assert(solana_delivery_client_install_topology(
               client,
               &installed.topology
           ) == SOLANA_DELIVERY_STATUS_OK);

    assert(clock_state.calls == 1U);
    assert(client->topology.received_monotonic_ns ==
           UINT64_C(123456789));

    solana_delivery_validator_t *old_validators =
        client->topology.validators;
    solana_delivery_endpoint_t *old_endpoints =
        client->topology.endpoints;
    solana_delivery_validator_endpoint_t *old_associations =
        client->topology.validator_endpoints;
    solana_delivery_leader_t *old_leaders =
        client->topology.leaders;

    const uint64_t old_generation =
        client->topology.view.generation;
    const uint64_t old_slot =
        client->topology.view.current_slot;
    const uint64_t old_receipt_time =
        client->topology.received_monotonic_ns;
    const uint8_t old_identity =
        client->topology.validators[0].identity.bytes[0];
    const uint16_t old_port =
        client->topology.endpoints[0].port;
    const uint64_t old_first_slot =
        client->topology.leaders[0].first_slot;
    const uint64_t old_last_slot =
        client->topology.leaders[0].last_slot;

    allocator_state.allocation_calls = 0U;
    allocator_state.free_calls = 0U;
    clock_state.calls = 0U;
    clock_state.fail = true;

    assert(solana_delivery_client_install_topology(
               client,
               &replacement.topology
           ) == SOLANA_DELIVERY_STATUS_INTERNAL_ERROR);

    assert(allocator_state.allocation_calls == 4U);
    assert(allocator_state.free_calls == 4U);
    assert(clock_state.calls == 1U);

    assert(client->has_topology);
    assert(client->topology.view.generation ==
           old_generation);
    assert(client->topology.view.current_slot ==
           old_slot);
    assert(client->topology.received_monotonic_ns ==
           old_receipt_time);

    assert(client->topology.validators ==
           old_validators);
    assert(client->topology.endpoints ==
           old_endpoints);
    assert(client->topology.validator_endpoints ==
           old_associations);
    assert(client->topology.leaders ==
           old_leaders);

    assert(
        client->topology.validators[0]
                .identity.bytes[0] ==
        old_identity
    );
    assert(client->topology.endpoints[0].port ==
           old_port);
    assert(client->topology.leaders[0].first_slot ==
           old_first_slot);
    assert(client->topology.leaders[0].last_slot ==
           old_last_slot);

    allocator_state.allocation_calls = 0U;
    allocator_state.free_calls = 0U;
    clock_state.calls = 0U;
    clock_state.fail = false;
    clock_state.value_ns = UINT64_C(987654321);

    assert(solana_delivery_client_install_topology(
               client,
               &replacement.topology
           ) == SOLANA_DELIVERY_STATUS_OK);

    assert(allocator_state.allocation_calls == 4U);
    assert(allocator_state.free_calls == 4U);
    assert(clock_state.calls == 1U);

    assert(client->topology.view.generation ==
           UINT64_C(51));
    assert(client->topology.view.current_slot ==
           UINT64_C(300));
    assert(client->topology.received_monotonic_ns ==
           UINT64_C(987654321));
    assert(
        client->topology.validators[0]
                .identity.bytes[0] ==
        UINT8_C(11)
    );
    assert(client->topology.endpoints[0].port ==
           UINT16_C(9002));
    assert(client->topology.leaders[0].first_slot ==
           UINT64_C(300));
    assert(client->topology.leaders[0].last_slot ==
           UINT64_C(304));

    solana_delivery_client_destroy(client);
}

int main(void) {
    test_client_lifecycle();
    test_deep_copy_and_normalization();
    test_generation_ordering();
    test_invalid_replacement_is_transactional();
    test_extended_input_is_normalized();
    test_empty_snapshot_and_receipt_time();
    test_null_install_arguments();
    test_allocation_failure_is_transactional();
    test_clock_failure_is_transactional();
    return 0;
}
