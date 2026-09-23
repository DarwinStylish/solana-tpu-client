// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/discovery.h"
#include "solana_delivery_internal.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
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
    size_t allocation_calls;
    size_t free_calls;
    size_t fail_on_call;
} allocator_state_t;

typedef struct {
    size_t calls;
    bool fail;
    uint64_t value_ns;
} clock_state_t;

typedef struct {
    const solana_delivery_topology_t *topology;
    solana_delivery_status_t acquire_status;
    size_t acquire_calls;
    size_t release_calls;
    const solana_delivery_topology_t *released_topology;
    solana_delivery_client_t *client;
    bool inspect_client_on_release;
    uint64_t expected_generation_on_release;
    bool mutate_fixture_on_release;
    fixture_t *mutable_fixture;
} provider_state_t;

typedef struct {
    solana_delivery_discovery_provider_t base;
    uint64_t extension;
} extended_provider_t;

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

static void *controlled_calloc(
    void *context,
    size_t count,
    size_t size
) {
    allocator_state_t *state = context;

    assert(state != NULL);

    ++state->allocation_calls;

    if (state->fail_on_call != 0U &&
        state->allocation_calls == state->fail_on_call) {
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

    if (state->fail) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    if (out_time_ns == NULL) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    *out_time_ns = state->value_ns;
    return SOLANA_DELIVERY_STATUS_OK;
}

static solana_delivery_status_t provider_acquire(
    void *context,
    const solana_delivery_topology_t **out_topology
) {
    provider_state_t *state = context;

    assert(state != NULL);
    assert(out_topology != NULL);
    assert(*out_topology == NULL);

    ++state->acquire_calls;
    *out_topology = state->topology;

    return state->acquire_status;
}

static void provider_release(
    void *context,
    const solana_delivery_topology_t *topology
) {
    provider_state_t *state = context;

    assert(state != NULL);
    assert(topology != NULL);
    assert(topology == state->topology);

    ++state->release_calls;
    state->released_topology = topology;

    if (state->inspect_client_on_release) {
        assert(state->client != NULL);
        assert(state->client->has_topology);
        assert(
            state->client->topology.view.generation ==
            state->expected_generation_on_release
        );
    }

    if (state->mutate_fixture_on_release) {
        assert(state->mutable_fixture != NULL);

        state->mutable_fixture
             ->validator
             .identity
             .bytes[0] = UINT8_C(99);

        state->mutable_fixture
             ->endpoint
             .port = UINT16_C(9999);
    }
}

static solana_delivery_discovery_provider_t
make_provider(provider_state_t *state) {
    solana_delivery_discovery_provider_t provider;

    memset(&provider, 0, sizeof(provider));

    provider.struct_size =
        (uint32_t)sizeof(provider);
    provider.flags =
        SOLANA_DELIVERY_DISCOVERY_PROVIDER_FLAGS_NONE;
    provider.context = state;
    provider.acquire = provider_acquire;
    provider.release = provider_release;

    return provider;
}

static solana_delivery_client_t *new_client(void) {
    solana_delivery_client_t *client = NULL;

    assert(
        solana_delivery_client_create(&client) ==
        SOLANA_DELIVERY_STATUS_OK
    );

    assert(client != NULL);
    assert(!client->has_topology);

    return client;
}

static solana_delivery_client_t *
new_client_with_dependencies(
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

    return client;
}

static void test_provider_validation(void) {
    fixture_t fixture;
    init_fixture(&fixture, UINT64_C(1));

    provider_state_t state = {
        .topology = &fixture.topology,
        .acquire_status = SOLANA_DELIVERY_STATUS_OK,
    };

    solana_delivery_discovery_provider_t provider =
        make_provider(&state);

    solana_delivery_client_t *client = new_client();

    assert(
        solana_delivery_client_refresh_topology(
            NULL,
            &provider
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(state.acquire_calls == 0U);

    assert(
        solana_delivery_client_refresh_topology(
            client,
            NULL
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(state.acquire_calls == 0U);

    provider.struct_size = (uint32_t)(
        offsetof(
            solana_delivery_discovery_provider_t,
            release
        ) +
        sizeof(provider.release) -
        1U
    );

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(state.acquire_calls == 0U);

    provider = make_provider(&state);
    provider.flags = UINT32_C(1);

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) == SOLANA_DELIVERY_STATUS_UNSUPPORTED
    );
    assert(state.acquire_calls == 0U);

    provider = make_provider(&state);
    provider.acquire = NULL;

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(state.acquire_calls == 0U);

    solana_delivery_client_destroy(client);
}

static void test_acquire_failure_and_null_success(void) {
    fixture_t fixture;
    init_fixture(&fixture, UINT64_C(1));

    provider_state_t state = {
        .topology = &fixture.topology,
        .acquire_status =
            SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED,
    };

    solana_delivery_discovery_provider_t provider =
        make_provider(&state);

    solana_delivery_client_t *client = new_client();

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) == SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED
    );

    assert(state.acquire_calls == 1U);
    assert(state.release_calls == 0U);
    assert(!client->has_topology);

    state.topology = NULL;
    state.acquire_status = SOLANA_DELIVERY_STATUS_OK;

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) == SOLANA_DELIVERY_STATUS_INTERNAL_ERROR
    );

    assert(state.acquire_calls == 2U);
    assert(state.release_calls == 0U);
    assert(!client->has_topology);

    solana_delivery_client_destroy(client);
}

static void test_success_and_optional_release(void) {
    fixture_t first;
    fixture_t second;

    init_fixture(&first, UINT64_C(5));
    init_fixture(&second, UINT64_C(6));

    provider_state_t state = {
        .topology = &first.topology,
        .acquire_status = SOLANA_DELIVERY_STATUS_OK,
    };

    solana_delivery_discovery_provider_t provider =
        make_provider(&state);

    provider.release = NULL;

    solana_delivery_client_t *client = new_client();

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(state.acquire_calls == 1U);
    assert(state.release_calls == 0U);
    assert(client->has_topology);
    assert(
        client->topology.view.generation ==
        UINT64_C(5)
    );

    state.topology = &second.topology;
    state.acquire_calls = 0U;
    state.release_calls = 0U;
    state.released_topology = NULL;
    state.client = client;
    state.inspect_client_on_release = true;
    state.expected_generation_on_release = UINT64_C(6);
    state.mutate_fixture_on_release = true;
    state.mutable_fixture = &second;

    provider = make_provider(&state);

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(state.acquire_calls == 1U);
    assert(state.release_calls == 1U);
    assert(
        state.released_topology ==
        &second.topology
    );

    assert(
        client->topology.view.generation ==
        UINT64_C(6)
    );

    assert(
        second.validator.identity.bytes[0] ==
        UINT8_C(99)
    );
    assert(
        second.endpoint.port ==
        UINT16_C(9999)
    );

    assert(
        client->topology
              .validators[0]
              .identity
              .bytes[0] ==
        UINT8_C(7)
    );
    assert(
        client->topology.endpoints[0].port ==
        UINT16_C(8003)
    );

    solana_delivery_client_destroy(client);
}

static void test_extended_provider_descriptor(void) {
    fixture_t fixture;
    init_fixture(&fixture, UINT64_C(1));

    provider_state_t state = {
        .topology = &fixture.topology,
        .acquire_status = SOLANA_DELIVERY_STATUS_OK,
    };

    extended_provider_t extended;
    memset(&extended, 0, sizeof(extended));

    extended.base = make_provider(&state);
    extended.base.struct_size =
        (uint32_t)sizeof(extended);
    extended.extension =
        UINT64_C(0x1122334455667788);

    solana_delivery_client_t *client = new_client();

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &extended.base
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(state.acquire_calls == 1U);
    assert(state.release_calls == 1U);
    assert(client->has_topology);
    assert(
        client->topology.view.generation ==
        UINT64_C(1)
    );

    solana_delivery_client_destroy(client);
}

static void test_install_rejection_releases_borrow(void) {
    fixture_t installed;
    fixture_t invalid;
    fixture_t stale;

    init_fixture(&installed, UINT64_C(10));
    init_fixture(&invalid, UINT64_C(11));
    init_fixture(&stale, UINT64_C(10));

    solana_delivery_client_t *client = new_client();

    assert(
        solana_delivery_client_install_topology(
            client,
            &installed.topology
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    solana_delivery_validator_t *old_validators =
        client->topology.validators;
    solana_delivery_endpoint_t *old_endpoints =
        client->topology.endpoints;
    const uint64_t old_receipt_time =
        client->topology.received_monotonic_ns;

    invalid.endpoint.port = UINT16_C(0);

    provider_state_t state = {
        .topology = &invalid.topology,
        .acquire_status = SOLANA_DELIVERY_STATUS_OK,
        .client = client,
        .inspect_client_on_release = true,
        .expected_generation_on_release = UINT64_C(10),
    };

    solana_delivery_discovery_provider_t provider =
        make_provider(&state);

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    assert(state.acquire_calls == 1U);
    assert(state.release_calls == 1U);
    assert(
        client->topology.view.generation ==
        UINT64_C(10)
    );
    assert(client->topology.validators == old_validators);
    assert(client->topology.endpoints == old_endpoints);
    assert(
        client->topology.received_monotonic_ns ==
        old_receipt_time
    );

    state.topology = &stale.topology;
    state.acquire_calls = 0U;
    state.release_calls = 0U;
    state.released_topology = NULL;

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) == SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE
    );

    assert(state.acquire_calls == 1U);
    assert(state.release_calls == 1U);
    assert(
        client->topology.view.generation ==
        UINT64_C(10)
    );
    assert(client->topology.validators == old_validators);
    assert(client->topology.endpoints == old_endpoints);
    assert(
        client->topology.received_monotonic_ns ==
        old_receipt_time
    );

    solana_delivery_client_destroy(client);
}

static void test_allocation_failures_release_borrow(void) {
    fixture_t installed;
    fixture_t replacement;

    init_fixture(&installed, UINT64_C(20));
    init_fixture(&replacement, UINT64_C(21));

    allocator_state_t allocator_state = {0};

    clock_state_t clock_state = {
        .calls = 0U,
        .fail = false,
        .value_ns = UINT64_C(1000),
    };

    solana_delivery_client_t *client =
        new_client_with_dependencies(
            &allocator_state,
            &clock_state
        );

    allocator_state.allocation_calls = 0U;
    allocator_state.free_calls = 0U;

    assert(
        solana_delivery_client_install_topology(
            client,
            &installed.topology
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    solana_delivery_validator_t *old_validators =
        client->topology.validators;
    solana_delivery_endpoint_t *old_endpoints =
        client->topology.endpoints;
    const uint64_t old_receipt_time =
        client->topology.received_monotonic_ns;

    provider_state_t state = {
        .topology = &replacement.topology,
        .acquire_status = SOLANA_DELIVERY_STATUS_OK,
        .client = client,
        .inspect_client_on_release = true,
        .expected_generation_on_release = UINT64_C(20),
    };

    solana_delivery_discovery_provider_t provider =
        make_provider(&state);

    for (size_t fail_on_call = 1U;
         fail_on_call <= 4U;
         ++fail_on_call) {
        allocator_state.allocation_calls = 0U;
        allocator_state.free_calls = 0U;
        allocator_state.fail_on_call = fail_on_call;

        state.acquire_calls = 0U;
        state.release_calls = 0U;
        state.released_topology = NULL;

        assert(
            solana_delivery_client_refresh_topology(
                client,
                &provider
            ) ==
            SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED
        );

        assert(
            allocator_state.allocation_calls ==
            fail_on_call
        );

        assert(state.acquire_calls == 1U);
        assert(state.release_calls == 1U);
        assert(
            state.released_topology ==
            &replacement.topology
        );

        assert(
            client->topology.view.generation ==
            UINT64_C(20)
        );
        assert(
            client->topology.validators ==
            old_validators
        );
        assert(
            client->topology.endpoints ==
            old_endpoints
        );
        assert(
            client->topology.received_monotonic_ns ==
            old_receipt_time
        );
    }

    allocator_state.fail_on_call = 0U;

    solana_delivery_client_destroy(client);
}

static void test_clock_failure_releases_borrow(void) {
    fixture_t installed;
    fixture_t replacement;

    init_fixture(&installed, UINT64_C(30));
    init_fixture(&replacement, UINT64_C(31));

    allocator_state_t allocator_state = {0};

    clock_state_t clock_state = {
        .calls = 0U,
        .fail = false,
        .value_ns = UINT64_C(2000),
    };

    solana_delivery_client_t *client =
        new_client_with_dependencies(
            &allocator_state,
            &clock_state
        );

    allocator_state.allocation_calls = 0U;
    allocator_state.free_calls = 0U;

    assert(
        solana_delivery_client_install_topology(
            client,
            &installed.topology
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    solana_delivery_validator_t *old_validators =
        client->topology.validators;
    solana_delivery_endpoint_t *old_endpoints =
        client->topology.endpoints;
    const uint64_t old_receipt_time =
        client->topology.received_monotonic_ns;

    allocator_state.allocation_calls = 0U;
    allocator_state.free_calls = 0U;
    clock_state.calls = 0U;
    clock_state.fail = true;

    provider_state_t state = {
        .topology = &replacement.topology,
        .acquire_status = SOLANA_DELIVERY_STATUS_OK,
        .client = client,
        .inspect_client_on_release = true,
        .expected_generation_on_release = UINT64_C(30),
    };

    solana_delivery_discovery_provider_t provider =
        make_provider(&state);

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) == SOLANA_DELIVERY_STATUS_INTERNAL_ERROR
    );

    assert(state.acquire_calls == 1U);
    assert(state.release_calls == 1U);
    assert(
        state.released_topology ==
        &replacement.topology
    );

    assert(allocator_state.allocation_calls == 4U);
    assert(allocator_state.free_calls == 4U);
    assert(clock_state.calls == 1U);

    assert(
        client->topology.view.generation ==
        UINT64_C(30)
    );
    assert(client->topology.validators == old_validators);
    assert(client->topology.endpoints == old_endpoints);
    assert(
        client->topology.received_monotonic_ns ==
        old_receipt_time
    );

    clock_state.fail = false;

    solana_delivery_client_destroy(client);
}

int main(void) {
    test_provider_validation();
    test_acquire_failure_and_null_success();
    test_success_and_optional_release();
    test_extended_provider_descriptor();
    test_install_rejection_releases_borrow();
    test_allocation_failures_release_borrow();
    test_clock_failure_releases_borrow();
    return 0;
}
