// SPDX-License-Identifier: Apache-2.0

#include "solana/discovery.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    solana_delivery_validator_t validator;
    solana_delivery_endpoint_t endpoint;
    solana_delivery_validator_endpoint_t association;
    solana_delivery_leader_t leader;
    solana_delivery_topology_t topology;
    size_t release_count;
} example_provider_state_t;

static void initialize_provider_state(
    example_provider_state_t *state
) {
    memset(state, 0, sizeof(*state));

    state->validator.struct_size =
        (uint32_t)sizeof(state->validator);

    state->endpoint.struct_size =
        (uint32_t)sizeof(state->endpoint);
    state->endpoint.address_family =
        SOLANA_DELIVERY_ADDRESS_IPV4;
    state->endpoint.transport =
        SOLANA_DELIVERY_TRANSPORT_QUIC;
    state->endpoint.role =
        SOLANA_DELIVERY_ENDPOINT_ROLE_TPU;
    state->endpoint.port = (uint16_t)8000;
    state->endpoint.address[0] = UINT8_C(127);
    state->endpoint.address[3] = UINT8_C(1);

    state->association.struct_size =
        (uint32_t)sizeof(state->association);

    state->leader.struct_size =
        (uint32_t)sizeof(state->leader);
    state->leader.first_slot = UINT64_C(10);
    state->leader.last_slot = UINT64_C(10);

    state->topology.struct_size =
        (uint32_t)sizeof(state->topology);
    state->topology.generation = UINT64_C(1);
    state->topology.current_slot = UINT64_C(10);
    state->topology.validators = &state->validator;
    state->topology.validator_count = UINT32_C(1);
    state->topology.validator_stride =
        (uint32_t)sizeof(state->validator);
    state->topology.endpoints = &state->endpoint;
    state->topology.endpoint_count = UINT32_C(1);
    state->topology.endpoint_stride =
        (uint32_t)sizeof(state->endpoint);
    state->topology.validator_endpoints =
        &state->association;
    state->topology.validator_endpoint_count = UINT32_C(1);
    state->topology.validator_endpoint_stride =
        (uint32_t)sizeof(state->association);
    state->topology.leaders = &state->leader;
    state->topology.leader_count = UINT32_C(1);
    state->topology.leader_stride =
        (uint32_t)sizeof(state->leader);
}

static solana_delivery_status_t acquire_topology(
    void *context,
    const solana_delivery_topology_t **out_topology
) {
    if (context == NULL || out_topology == NULL) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    example_provider_state_t *state =
        (example_provider_state_t *)context;
    *out_topology = &state->topology;
    return SOLANA_DELIVERY_STATUS_OK;
}

static void release_topology(
    void *context,
    const solana_delivery_topology_t *topology
) {
    example_provider_state_t *state =
        (example_provider_state_t *)context;

    if (state != NULL && topology == &state->topology) {
        state->release_count += 1U;
    }
}

int main(void) {
    example_provider_state_t state;
    initialize_provider_state(&state);

    solana_delivery_discovery_provider_t provider = {0};
    provider.struct_size = (uint32_t)sizeof(provider);
    provider.flags =
        SOLANA_DELIVERY_DISCOVERY_PROVIDER_FLAGS_NONE;
    provider.context = &state;
    provider.acquire = acquire_topology;
    provider.release = release_topology;

    solana_delivery_client_t *client = NULL;

    if (solana_delivery_client_create(&client) !=
        SOLANA_DELIVERY_STATUS_OK) {
        return 1;
    }

    const solana_delivery_status_t status =
        solana_delivery_client_refresh_topology(
            client,
            &provider
        );

    solana_delivery_client_destroy(client);

    return status == SOLANA_DELIVERY_STATUS_OK &&
                   state.release_count == 1U
               ? 0
               : 1;
}
