// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/discovery.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    solana_delivery_validator_t validator;
    solana_delivery_endpoint_t endpoint;
    solana_delivery_validator_endpoint_t association;
    solana_delivery_leader_t leader;
    solana_delivery_topology_t topology;
} fixture_t;

typedef struct {
    fixture_t *fixture;
    solana_delivery_status_t acquire_status;
    size_t acquire_calls;
    size_t release_calls;
    const solana_delivery_topology_t *released_topology;
    bool invalidate_route_on_release;
} provider_state_t;

static const solana_delivery_topology_t *
    null_context_topology = NULL;
static size_t null_context_acquire_calls = 0U;

static void init_routable_fixture(
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
    fixture->association.validator_index =
        UINT32_C(0);
    fixture->association.endpoint_index =
        UINT32_C(0);

    fixture->leader.struct_size =
        (uint32_t)sizeof(fixture->leader);
    fixture->leader.validator_index =
        UINT32_C(0);
    fixture->leader.first_slot = UINT64_C(100);
    fixture->leader.last_slot = UINT64_C(104);

    fixture->topology.struct_size =
        (uint32_t)sizeof(fixture->topology);
    fixture->topology.generation = generation;
    fixture->topology.current_slot = UINT64_C(100);

    fixture->topology.validators =
        &fixture->validator;
    fixture->topology.validator_count =
        UINT32_C(1);
    fixture->topology.validator_stride =
        (uint32_t)sizeof(fixture->validator);

    fixture->topology.endpoints =
        &fixture->endpoint;
    fixture->topology.endpoint_count =
        UINT32_C(1);
    fixture->topology.endpoint_stride =
        (uint32_t)sizeof(fixture->endpoint);

    fixture->topology.validator_endpoints =
        &fixture->association;
    fixture->topology.validator_endpoint_count =
        UINT32_C(1);
    fixture->topology.validator_endpoint_stride =
        (uint32_t)sizeof(fixture->association);

    fixture->topology.leaders =
        &fixture->leader;
    fixture->topology.leader_count =
        UINT32_C(1);
    fixture->topology.leader_stride =
        (uint32_t)sizeof(fixture->leader);
}

static void init_empty_fixture(
    fixture_t *fixture,
    uint64_t generation
) {
    memset(fixture, 0, sizeof(*fixture));

    fixture->topology.struct_size =
        (uint32_t)sizeof(fixture->topology);
    fixture->topology.generation = generation;
    fixture->topology.current_slot = UINT64_C(100);
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

    if (state->acquire_status ==
        SOLANA_DELIVERY_STATUS_OK) {
        assert(state->fixture != NULL);
        *out_topology = &state->fixture->topology;
    }

    return state->acquire_status;
}

static void provider_release(
    void *context,
    const solana_delivery_topology_t *topology
) {
    provider_state_t *state = context;

    assert(state != NULL);
    assert(state->fixture != NULL);
    assert(topology == &state->fixture->topology);

    ++state->release_calls;
    state->released_topology = topology;

    if (state->invalidate_route_on_release) {
        state->fixture->leader.first_slot =
            UINT64_C(1000);
        state->fixture->leader.last_slot =
            UINT64_C(1001);
        state->fixture->endpoint.port =
            UINT16_C(1);
        state->fixture->validator
             .identity
             .bytes[0] = UINT8_C(99);
    }
}

static solana_delivery_status_t
null_context_acquire(
    void *context,
    const solana_delivery_topology_t **out_topology
) {
    assert(context == NULL);
    assert(out_topology != NULL);
    assert(*out_topology == NULL);
    assert(null_context_topology != NULL);

    ++null_context_acquire_calls;
    *out_topology = null_context_topology;

    return SOLANA_DELIVERY_STATUS_OK;
}

static solana_delivery_discovery_provider_t
make_provider(provider_state_t *state) {
    return (solana_delivery_discovery_provider_t){
        .struct_size =
            (uint32_t)sizeof(
                solana_delivery_discovery_provider_t
            ),
        .flags =
            SOLANA_DELIVERY_DISCOVERY_PROVIDER_FLAGS_NONE,
        .context = state,
        .acquire = provider_acquire,
        .release = provider_release,
    };
}

static solana_delivery_submit_options_t
valid_options(void) {
    return (solana_delivery_submit_options_t){
        .struct_size =
            (uint32_t)sizeof(
                solana_delivery_submit_options_t
            ),
        .flags = SOLANA_DELIVERY_SUBMIT_FLAGS_NONE,
        .max_topology_age_ns = UINT64_MAX,
        .target_limit = UINT32_C(1),
        .reserved0 = UINT32_C(0),
    };
}

static solana_delivery_client_t *new_client(void) {
    solana_delivery_client_t *client = NULL;

    assert(
        solana_delivery_client_create(&client) ==
        SOLANA_DELIVERY_STATUS_OK
    );
    assert(client != NULL);

    return client;
}

static solana_delivery_request_id_t
submit_and_expect_accepted(
    solana_delivery_client_t *client,
    uint8_t marker
) {
    const uint8_t transaction[] = {
        marker,
        UINT8_C(2),
        UINT8_C(3),
        UINT8_C(4),
    };

    solana_delivery_submit_options_t options =
        valid_options();

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

    assert(
        request_id !=
        SOLANA_DELIVERY_REQUEST_ID_NONE
    );

    solana_delivery_event_t event = {0};
    size_t event_count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            &event,
            1U,
            (uint32_t)sizeof(event),
            &event_count
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(event_count == 1U);
    assert(
        event.struct_size ==
        (uint32_t)sizeof(solana_delivery_event_t)
    );
    assert(
        event.event_class ==
        SOLANA_DELIVERY_EVENT_CLASS_REQUEST
    );
    assert(
        event.event_code ==
        SOLANA_DELIVERY_REQUEST_EVENT_ACCEPTED
    );
    assert(event.diagnostic_code == 0);
    assert(event.request_id == request_id);
    assert(
        event.attempt_id ==
        SOLANA_DELIVERY_ATTEMPT_ID_NONE
    );
    assert(
        event.request_sequence ==
        UINT64_C(1)
    );
    assert(event.reserved[0] == UINT64_C(0));
    assert(event.reserved[1] == UINT64_C(0));

    event_count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            &event,
            1U,
            (uint32_t)sizeof(event),
            &event_count
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(event_count == 0U);

    return request_id;
}

static void submit_and_expect_unavailable(
    solana_delivery_client_t *client
) {
    const uint8_t transaction[] = {
        UINT8_C(9),
        UINT8_C(8),
        UINT8_C(7),
    };

    solana_delivery_submit_options_t options =
        valid_options();

    solana_delivery_request_id_t request_id =
        UINT64_C(77);

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

    assert(
        request_id ==
        SOLANA_DELIVERY_REQUEST_ID_NONE
    );

    solana_delivery_event_t event = {0};
    size_t event_count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            &event,
            1U,
            (uint32_t)sizeof(event),
            &event_count
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(event_count == 0U);
}

static void test_misaligned_provider_rejected(void) {
    solana_delivery_client_t *client = new_client();

    _Alignas(solana_delivery_discovery_provider_t)
    uint8_t storage[
        sizeof(solana_delivery_discovery_provider_t) +
        1U
    ];

    memset(storage, 0, sizeof(storage));

    const solana_delivery_discovery_provider_t
        *misaligned_provider =
            (const solana_delivery_discovery_provider_t *)
                (const void *)(storage + 1U);

    assert(
        solana_delivery_client_refresh_topology(
            client,
            misaligned_provider
        ) ==
        SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    solana_delivery_client_destroy(client);
}

static void test_null_context_is_valid(void) {
    fixture_t fixture;
    init_routable_fixture(&fixture, UINT64_C(1));

    null_context_topology = &fixture.topology;
    null_context_acquire_calls = 0U;

    solana_delivery_discovery_provider_t provider = {
        .struct_size =
            (uint32_t)sizeof(provider),
        .flags =
            SOLANA_DELIVERY_DISCOVERY_PROVIDER_FLAGS_NONE,
        .context = NULL,
        .acquire = null_context_acquire,
        .release = NULL,
    };

    solana_delivery_client_t *client = new_client();

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(null_context_acquire_calls == 1U);

    assert(
        submit_and_expect_accepted(
            client,
            UINT8_C(11)
        ) != SOLANA_DELIVERY_REQUEST_ID_NONE
    );

    solana_delivery_client_destroy(client);

    null_context_topology = NULL;
    null_context_acquire_calls = 0U;
}

static void
test_refresh_internalizes_provider_snapshot(void) {
    fixture_t fixture;
    init_routable_fixture(&fixture, UINT64_C(5));

    provider_state_t state = {
        .fixture = &fixture,
        .acquire_status = SOLANA_DELIVERY_STATUS_OK,
        .invalidate_route_on_release = true,
    };

    solana_delivery_discovery_provider_t provider =
        make_provider(&state);

    solana_delivery_client_t *client = new_client();

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
        &fixture.topology
    );

    assert(
        fixture.leader.first_slot ==
        UINT64_C(1000)
    );

    assert(
        submit_and_expect_accepted(
            client,
            UINT8_C(21)
        ) != SOLANA_DELIVERY_REQUEST_ID_NONE
    );

    solana_delivery_client_destroy(client);
}

static void
test_stale_refresh_preserves_previous_topology(void) {
    fixture_t installed;
    fixture_t stale;

    init_routable_fixture(&installed, UINT64_C(10));
    init_routable_fixture(&stale, UINT64_C(10));

    provider_state_t state = {
        .fixture = &installed,
        .acquire_status = SOLANA_DELIVERY_STATUS_OK,
    };

    solana_delivery_discovery_provider_t provider =
        make_provider(&state);

    solana_delivery_client_t *client = new_client();

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(
        submit_and_expect_accepted(
            client,
            UINT8_C(31)
        ) != SOLANA_DELIVERY_REQUEST_ID_NONE
    );

    state.fixture = &stale;
    state.acquire_calls = 0U;
    state.release_calls = 0U;
    state.released_topology = NULL;

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) ==
        SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE
    );

    assert(state.acquire_calls == 1U);
    assert(state.release_calls == 1U);
    assert(
        state.released_topology ==
        &stale.topology
    );

    assert(
        submit_and_expect_accepted(
            client,
            UINT8_C(32)
        ) != SOLANA_DELIVERY_REQUEST_ID_NONE
    );

    solana_delivery_client_destroy(client);
}

static void
test_invalid_refresh_preserves_previous_topology(void) {
    fixture_t installed;
    fixture_t invalid;

    init_routable_fixture(&installed, UINT64_C(20));
    init_routable_fixture(&invalid, UINT64_C(21));

    invalid.endpoint.port = UINT16_C(0);

    provider_state_t state = {
        .fixture = &installed,
        .acquire_status = SOLANA_DELIVERY_STATUS_OK,
    };

    solana_delivery_discovery_provider_t provider =
        make_provider(&state);

    solana_delivery_client_t *client = new_client();

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(
        submit_and_expect_accepted(
            client,
            UINT8_C(41)
        ) != SOLANA_DELIVERY_REQUEST_ID_NONE
    );

    state.fixture = &invalid;
    state.acquire_calls = 0U;
    state.release_calls = 0U;
    state.released_topology = NULL;

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) ==
        SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    assert(state.acquire_calls == 1U);
    assert(state.release_calls == 1U);
    assert(
        state.released_topology ==
        &invalid.topology
    );

    assert(
        submit_and_expect_accepted(
            client,
            UINT8_C(42)
        ) != SOLANA_DELIVERY_REQUEST_ID_NONE
    );

    solana_delivery_client_destroy(client);
}

static void
test_empty_refresh_replaces_routable_topology(void) {
    fixture_t routable;
    fixture_t empty;

    init_routable_fixture(&routable, UINT64_C(30));
    init_empty_fixture(&empty, UINT64_C(31));

    provider_state_t state = {
        .fixture = &routable,
        .acquire_status = SOLANA_DELIVERY_STATUS_OK,
    };

    solana_delivery_discovery_provider_t provider =
        make_provider(&state);

    solana_delivery_client_t *client = new_client();

    assert(
        solana_delivery_client_refresh_topology(
            client,
            &provider
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(
        submit_and_expect_accepted(
            client,
            UINT8_C(51)
        ) != SOLANA_DELIVERY_REQUEST_ID_NONE
    );

    state.fixture = &empty;
    state.acquire_calls = 0U;
    state.release_calls = 0U;
    state.released_topology = NULL;

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
        &empty.topology
    );

    submit_and_expect_unavailable(client);

    solana_delivery_client_destroy(client);
}

int main(void) {
    test_misaligned_provider_rejected();
    test_null_context_is_valid();
    test_refresh_internalizes_provider_snapshot();
    test_stale_refresh_preserves_previous_topology();
    test_invalid_refresh_preserves_previous_topology();
    test_empty_refresh_replaces_routable_topology();
    return 0;
}
