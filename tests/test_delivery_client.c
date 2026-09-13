// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/delivery.h"
#include "solana_delivery_internal.h"

#include <assert.h>
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
    solana_delivery_validator_t base;
    uint64_t extension;
} extended_validator_t;

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

int main(void) {
    test_client_lifecycle();
    test_deep_copy_and_normalization();
    test_generation_ordering();
    test_invalid_replacement_is_transactional();
    test_extended_input_is_normalized();
    test_empty_snapshot_and_receipt_time();
    test_null_install_arguments();
    return 0;
}
