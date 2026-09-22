// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/delivery.h"
#include "solana_delivery_internal.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    solana_delivery_validator_t validators[2];
    solana_delivery_endpoint_t endpoints[3];
    solana_delivery_validator_endpoint_t associations[4];
    solana_delivery_leader_t leaders[2];
    solana_delivery_topology_t topology;
} resolution_fixture_t;

static void init_fixture(
    resolution_fixture_t *fixture
) {
    memset(fixture, 0, sizeof(*fixture));

    for (uint32_t i = 0U; i < UINT32_C(2); ++i) {
        fixture->validators[i].struct_size =
            (uint32_t)sizeof(fixture->validators[i]);
        fixture->validators[i].identity.bytes[0] =
            (uint8_t)(UINT8_C(10) + (uint8_t)i);
    }

    for (uint32_t i = 0U; i < UINT32_C(3); ++i) {
        fixture->endpoints[i].struct_size =
            (uint32_t)sizeof(fixture->endpoints[i]);
        fixture->endpoints[i].address_family =
            SOLANA_DELIVERY_ADDRESS_IPV4;
        fixture->endpoints[i].transport =
            SOLANA_DELIVERY_TRANSPORT_QUIC;
        fixture->endpoints[i].role =
            SOLANA_DELIVERY_ENDPOINT_ROLE_TPU;
        fixture->endpoints[i].port =
            (uint16_t)(UINT16_C(8000) + (uint16_t)i);
        fixture->endpoints[i].address[0] = UINT8_C(127);
        fixture->endpoints[i].address[3] =
            (uint8_t)(i + UINT32_C(1));
    }

    for (uint32_t i = 0U; i < UINT32_C(4); ++i) {
        fixture->associations[i].struct_size =
            (uint32_t)sizeof(fixture->associations[i]);
    }

    fixture->associations[0].validator_index = UINT32_C(0);
    fixture->associations[0].endpoint_index = UINT32_C(1);
    fixture->associations[1].validator_index = UINT32_C(1);
    fixture->associations[1].endpoint_index = UINT32_C(2);
    fixture->associations[2].validator_index = UINT32_C(1);
    fixture->associations[2].endpoint_index = UINT32_C(0);
    fixture->associations[3].validator_index = UINT32_C(0);
    fixture->associations[3].endpoint_index = UINT32_C(0);

    for (uint32_t i = 0U; i < UINT32_C(2); ++i) {
        fixture->leaders[i].struct_size =
            (uint32_t)sizeof(fixture->leaders[i]);
    }

    fixture->leaders[0].validator_index = UINT32_C(1);
    fixture->leaders[0].first_slot = UINT64_C(100);
    fixture->leaders[0].last_slot = UINT64_C(105);

    fixture->leaders[1].validator_index = UINT32_C(0);
    fixture->leaders[1].first_slot = UINT64_C(103);
    fixture->leaders[1].last_slot = UINT64_C(108);

    fixture->topology.struct_size =
        (uint32_t)sizeof(fixture->topology);
    fixture->topology.generation = UINT64_C(1);
    fixture->topology.current_slot = UINT64_C(103);

    fixture->topology.validators = fixture->validators;
    fixture->topology.validator_count = UINT32_C(2);
    fixture->topology.validator_stride =
        (uint32_t)sizeof(fixture->validators[0]);

    fixture->topology.endpoints = fixture->endpoints;
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

static solana_delivery_client_t *new_client(void) {
    solana_delivery_client_t *client = NULL;

    assert(
        solana_delivery_client_create(&client) ==
        SOLANA_DELIVERY_STATUS_OK
    );
    assert(client != NULL);

    return client;
}

static solana_delivery_client_t *
new_client_with_fixture(
    resolution_fixture_t *fixture
) {
    solana_delivery_client_t *client = new_client();

    assert(
        solana_delivery_client_install_topology(
            client,
            &fixture->topology
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    return client;
}

static void assert_candidate(
    const solana_delivery_topology_candidate_t *candidate,
    uint32_t leader_index,
    uint32_t validator_index,
    uint32_t endpoint_index
) {
    assert(candidate->leader_index == leader_index);
    assert(candidate->validator_index == validator_index);
    assert(candidate->endpoint_index == endpoint_index);
}

static void test_argument_contract(void) {
    resolution_fixture_t fixture;
    init_fixture(&fixture);

    solana_delivery_client_t *client =
        new_client_with_fixture(&fixture);

    solana_delivery_topology_candidate_t candidate;
    size_t count = SIZE_MAX;

    assert(
        solana_delivery_client_resolve_slot(
            NULL,
            UINT64_C(103),
            &candidate,
            1U,
            &count
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(count == 0U);

    assert(
        solana_delivery_client_resolve_slot(
            client,
            UINT64_C(103),
            &candidate,
            1U,
            NULL
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    count = SIZE_MAX;
    assert(
        solana_delivery_client_resolve_slot(
            client,
            UINT64_C(103),
            NULL,
            1U,
            &count
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(count == 0U);

    solana_delivery_client_destroy(client);
}

static void test_missing_topology(void) {
    solana_delivery_client_t *client = new_client();
    size_t count = SIZE_MAX;

    assert(
        solana_delivery_client_resolve_slot(
            client,
            UINT64_C(100),
            NULL,
            0U,
            &count
        ) ==
        SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE
    );
    assert(count == 0U);

    solana_delivery_client_destroy(client);
}

static void test_boundary_and_no_route_semantics(void) {
    resolution_fixture_t fixture;
    init_fixture(&fixture);

    solana_delivery_client_t *client =
        new_client_with_fixture(&fixture);

    solana_delivery_topology_candidate_t candidates[4];
    size_t count = 0U;

    assert(
        solana_delivery_client_resolve_slot(
            client,
            UINT64_C(99),
            candidates,
            4U,
            &count
        ) ==
        SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE
    );
    assert(count == 0U);

    assert(
        solana_delivery_client_resolve_slot(
            client,
            UINT64_C(100),
            candidates,
            4U,
            &count
        ) == SOLANA_DELIVERY_STATUS_OK
    );
    assert(count == 2U);
    assert_candidate(
        &candidates[0],
        UINT32_C(0),
        UINT32_C(1),
        UINT32_C(2)
    );
    assert_candidate(
        &candidates[1],
        UINT32_C(0),
        UINT32_C(1),
        UINT32_C(0)
    );

    assert(
        solana_delivery_client_resolve_slot(
            client,
            UINT64_C(108),
            candidates,
            4U,
            &count
        ) == SOLANA_DELIVERY_STATUS_OK
    );
    assert(count == 2U);
    assert_candidate(
        &candidates[0],
        UINT32_C(1),
        UINT32_C(0),
        UINT32_C(1)
    );
    assert_candidate(
        &candidates[1],
        UINT32_C(1),
        UINT32_C(0),
        UINT32_C(0)
    );

    assert(
        solana_delivery_client_resolve_slot(
            client,
            UINT64_C(109),
            candidates,
            4U,
            &count
        ) ==
        SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE
    );
    assert(count == 0U);

    solana_delivery_client_destroy(client);
}

static void test_deterministic_source_order(void) {
    resolution_fixture_t fixture;
    init_fixture(&fixture);

    solana_delivery_client_t *client =
        new_client_with_fixture(&fixture);

    solana_delivery_topology_candidate_t candidates[4];
    size_t count = 0U;

    assert(
        solana_delivery_client_resolve_slot(
            client,
            UINT64_C(103),
            candidates,
            4U,
            &count
        ) == SOLANA_DELIVERY_STATUS_OK
    );
    assert(count == 4U);

    assert_candidate(
        &candidates[0],
        UINT32_C(0),
        UINT32_C(1),
        UINT32_C(2)
    );
    assert_candidate(
        &candidates[1],
        UINT32_C(0),
        UINT32_C(1),
        UINT32_C(0)
    );
    assert_candidate(
        &candidates[2],
        UINT32_C(1),
        UINT32_C(0),
        UINT32_C(1)
    );
    assert_candidate(
        &candidates[3],
        UINT32_C(1),
        UINT32_C(0),
        UINT32_C(0)
    );

    solana_delivery_client_destroy(client);
}

static void test_duplicate_relationships_are_preserved(void) {
    resolution_fixture_t fixture;
    init_fixture(&fixture);

    fixture.associations[2].endpoint_index = UINT32_C(2);

    solana_delivery_client_t *client =
        new_client_with_fixture(&fixture);

    solana_delivery_topology_candidate_t candidates[2];
    size_t count = 0U;

    assert(
        solana_delivery_client_resolve_slot(
            client,
            UINT64_C(100),
            candidates,
            2U,
            &count
        ) == SOLANA_DELIVERY_STATUS_OK
    );
    assert(count == 2U);

    assert_candidate(
        &candidates[0],
        UINT32_C(0),
        UINT32_C(1),
        UINT32_C(2)
    );
    assert_candidate(
        &candidates[1],
        UINT32_C(0),
        UINT32_C(1),
        UINT32_C(2)
    );

    solana_delivery_client_destroy(client);
}

static void test_no_associated_endpoint(void) {
    resolution_fixture_t fixture;
    init_fixture(&fixture);

    fixture.topology.validator_endpoint_count = 0U;

    solana_delivery_client_t *client =
        new_client_with_fixture(&fixture);

    size_t count = SIZE_MAX;

    assert(
        solana_delivery_client_resolve_slot(
            client,
            UINT64_C(100),
            NULL,
            0U,
            &count
        ) ==
        SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE
    );
    assert(count == 0U);

    solana_delivery_client_destroy(client);
}

static void test_capacity_query_and_atomicity(void) {
    resolution_fixture_t fixture;
    init_fixture(&fixture);

    solana_delivery_client_t *client =
        new_client_with_fixture(&fixture);

    size_t count = 0U;

    assert(
        solana_delivery_client_resolve_slot(
            client,
            UINT64_C(103),
            NULL,
            0U,
            &count
        ) ==
        SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED
    );
    assert(count == 4U);

    solana_delivery_topology_candidate_t candidates[4];
    solana_delivery_topology_candidate_t before[4];

    memset(candidates, 0xA5, sizeof(candidates));
    memcpy(before, candidates, sizeof(before));

    assert(
        solana_delivery_client_resolve_slot(
            client,
            UINT64_C(103),
            candidates,
            3U,
            &count
        ) ==
        SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED
    );
    assert(count == 4U);
    assert(memcmp(
               candidates,
               before,
               sizeof(candidates)
           ) == 0);

    assert(
        solana_delivery_client_resolve_slot(
            client,
            UINT64_C(103),
            candidates,
            4U,
            &count
        ) == SOLANA_DELIVERY_STATUS_OK
    );
    assert(count == 4U);

    solana_delivery_client_destroy(client);
}

static void test_installed_empty_topology(void) {
    solana_delivery_topology_t topology;
    memset(&topology, 0, sizeof(topology));
    topology.struct_size = (uint32_t)sizeof(topology);
    topology.generation = UINT64_C(1);

    solana_delivery_client_t *client = new_client();

    assert(
        solana_delivery_client_install_topology(
            client,
            &topology
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    size_t count = SIZE_MAX;

    assert(
        solana_delivery_client_resolve_slot(
            client,
            UINT64_C(0),
            NULL,
            0U,
            &count
        ) ==
        SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE
    );
    assert(count == 0U);

    solana_delivery_client_destroy(client);
}

int main(void) {
    test_argument_contract();
    test_missing_topology();
    test_boundary_and_no_route_semantics();
    test_deterministic_source_order();
    test_duplicate_relationships_are_preserved();
    test_no_associated_endpoint();
    test_capacity_query_and_atomicity();
    test_installed_empty_topology();
    return 0;
}
