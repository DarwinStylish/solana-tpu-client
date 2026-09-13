// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/delivery.h"

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

static void init_valid_fixture(fixture_t *fixture) {
    memset(fixture, 0, sizeof(*fixture));

    fixture->validator.struct_size =
        (uint32_t)sizeof(fixture->validator);
    fixture->validator.identity.bytes[0] = UINT8_C(1);

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
    fixture->topology.generation = UINT64_C(1);
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
    fixture->topology.validator_endpoint_count = UINT32_C(1);
    fixture->topology.validator_endpoint_stride =
        (uint32_t)sizeof(fixture->association);
    fixture->topology.leaders = &fixture->leader;
    fixture->topology.leader_count = UINT32_C(1);
    fixture->topology.leader_stride =
        (uint32_t)sizeof(fixture->leader);
}

static void test_valid_topology(void) {
    fixture_t fixture;
    init_valid_fixture(&fixture);

    assert(fixture.topology.validators == &fixture.validator);
    assert(fixture.topology.endpoints == &fixture.endpoint);
    assert(
        fixture.topology.validator_endpoints ==
        &fixture.association
    );
    assert(fixture.topology.leaders == &fixture.leader);

    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_OK);

    fixture.topology.struct_size += UINT32_C(16);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_OK);
}

static void test_top_level_contract(void) {
    fixture_t fixture;
    init_valid_fixture(&fixture);

    assert(solana_delivery_topology_validate(NULL) ==
           SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    fixture.topology.struct_size =
        (uint32_t)sizeof(fixture.topology) - UINT32_C(1);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    init_valid_fixture(&fixture);
    fixture.topology.reserved0 = UINT32_C(1);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    init_valid_fixture(&fixture);
    fixture.topology.validators = NULL;
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);
}

static void test_stride_contract(void) {
    fixture_t fixture;
    init_valid_fixture(&fixture);

    fixture.topology.validator_stride =
        (uint32_t)sizeof(fixture.validator) - UINT32_C(1);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    init_valid_fixture(&fixture);
    fixture.topology.validator_stride =
        (uint32_t)sizeof(fixture.validator) + UINT32_C(1);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    init_valid_fixture(&fixture);
    fixture.validator.struct_size =
        (uint32_t)sizeof(fixture.validator) + UINT32_C(8);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);
}

static void test_extended_stride(void) {
    extended_validator_t validators[2];
    memset(validators, 0, sizeof(validators));

    for (size_t i = 0U; i < 2U; ++i) {
        validators[i].base.struct_size =
            (uint32_t)sizeof(validators[i]);
        validators[i].base.identity.bytes[0] =
            (uint8_t)(i + 1U);
        validators[i].extension = UINT64_C(0xA5A5A5A5);
    }

    solana_delivery_topology_t topology;
    memset(&topology, 0, sizeof(topology));
    topology.struct_size = (uint32_t)sizeof(topology);
    topology.validators =
        (const solana_delivery_validator_t *)validators;
    topology.validator_count = UINT32_C(2);
    topology.validator_stride =
        (uint32_t)sizeof(validators[0]);

    assert(solana_delivery_topology_validate(&topology) ==
           SOLANA_DELIVERY_STATUS_OK);
}

static void test_validator_contract(void) {
    fixture_t fixture;
    init_valid_fixture(&fixture);

    fixture.validator.struct_size =
        (uint32_t)sizeof(fixture.validator) - UINT32_C(1);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    init_valid_fixture(&fixture);
    fixture.validator.reserved0 = UINT32_C(1);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    init_valid_fixture(&fixture);
    fixture.validator.reserved1[7] = UINT8_C(1);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);
}

static void test_endpoint_contract(void) {
    fixture_t fixture;
    init_valid_fixture(&fixture);

    fixture.endpoint.port = UINT16_C(0);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    init_valid_fixture(&fixture);
    fixture.endpoint.address_family =
        SOLANA_DELIVERY_ADDRESS_UNSPEC;
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    init_valid_fixture(&fixture);
    fixture.endpoint.address_family = UINT8_C(99);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_UNSUPPORTED);

    init_valid_fixture(&fixture);
    fixture.endpoint.transport =
        SOLANA_DELIVERY_TRANSPORT_UNSPEC;
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    init_valid_fixture(&fixture);
    fixture.endpoint.transport = UINT8_C(99);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_UNSUPPORTED);

    init_valid_fixture(&fixture);
    fixture.endpoint.role =
        SOLANA_DELIVERY_ENDPOINT_ROLE_UNSPEC;
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    init_valid_fixture(&fixture);
    fixture.endpoint.role = UINT8_C(99);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_UNSUPPORTED);

    init_valid_fixture(&fixture);
    fixture.endpoint.address[15] = UINT8_C(1);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    init_valid_fixture(&fixture);
    fixture.endpoint.address_family =
        SOLANA_DELIVERY_ADDRESS_IPV6;
    fixture.endpoint.address[15] = UINT8_C(1);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_OK);
}

static void test_reference_contract(void) {
    fixture_t fixture;
    init_valid_fixture(&fixture);

    fixture.association.validator_index = UINT32_C(1);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    init_valid_fixture(&fixture);
    fixture.association.endpoint_index = UINT32_C(1);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    init_valid_fixture(&fixture);
    fixture.leader.validator_index = UINT32_C(1);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);

    init_valid_fixture(&fixture);
    fixture.leader.first_slot = UINT64_C(105);
    fixture.leader.last_slot = UINT64_C(104);
    assert(solana_delivery_topology_validate(
               &fixture.topology
           ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT);
}

static void test_empty_topology(void) {
    solana_delivery_topology_t topology;
    memset(&topology, 0, sizeof(topology));
    topology.struct_size = (uint32_t)sizeof(topology);

    assert(solana_delivery_topology_validate(&topology) ==
           SOLANA_DELIVERY_STATUS_OK);
}

int main(void) {
    test_valid_topology();
    test_top_level_contract();
    test_stride_contract();
    test_extended_stride();
    test_validator_contract();
    test_endpoint_contract();
    test_reference_contract();
    test_empty_topology();
    return 0;
}
