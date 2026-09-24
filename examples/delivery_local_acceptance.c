// SPDX-License-Identifier: Apache-2.0

#include "solana/delivery.h"

#include <stddef.h>
#include <stdint.h>

int main(void) {
    solana_delivery_validator_t validator = {0};
    validator.struct_size = (uint32_t)sizeof(validator);

    solana_delivery_endpoint_t endpoint = {0};
    endpoint.struct_size = (uint32_t)sizeof(endpoint);
    endpoint.address_family = SOLANA_DELIVERY_ADDRESS_IPV4;
    endpoint.transport = SOLANA_DELIVERY_TRANSPORT_QUIC;
    endpoint.role = SOLANA_DELIVERY_ENDPOINT_ROLE_TPU;
    endpoint.port = (uint16_t)8000;
    endpoint.address[0] = UINT8_C(127);
    endpoint.address[3] = UINT8_C(1);

    solana_delivery_validator_endpoint_t association = {0};
    association.struct_size = (uint32_t)sizeof(association);
    association.validator_index = UINT32_C(0);
    association.endpoint_index = UINT32_C(0);

    solana_delivery_leader_t leader = {0};
    leader.struct_size = (uint32_t)sizeof(leader);
    leader.validator_index = UINT32_C(0);
    leader.first_slot = UINT64_C(10);
    leader.last_slot = UINT64_C(10);

    solana_delivery_topology_t topology = {0};
    topology.struct_size = (uint32_t)sizeof(topology);
    topology.generation = UINT64_C(1);
    topology.current_slot = UINT64_C(10);
    topology.validators = &validator;
    topology.validator_count = UINT32_C(1);
    topology.validator_stride = (uint32_t)sizeof(validator);
    topology.endpoints = &endpoint;
    topology.endpoint_count = UINT32_C(1);
    topology.endpoint_stride = (uint32_t)sizeof(endpoint);
    topology.validator_endpoints = &association;
    topology.validator_endpoint_count = UINT32_C(1);
    topology.validator_endpoint_stride =
        (uint32_t)sizeof(association);
    topology.leaders = &leader;
    topology.leader_count = UINT32_C(1);
    topology.leader_stride = (uint32_t)sizeof(leader);

    solana_delivery_client_t *client = NULL;

    if (solana_delivery_client_create(&client) !=
        SOLANA_DELIVERY_STATUS_OK) {
        return 1;
    }

    if (solana_delivery_client_install_topology(
            client,
            &topology) != SOLANA_DELIVERY_STATUS_OK) {
        solana_delivery_client_destroy(client);
        return 1;
    }

    const uint8_t transaction[] = {UINT8_C(1)};

    solana_delivery_submit_options_t options = {0};
    options.struct_size = (uint32_t)sizeof(options);
    options.flags = SOLANA_DELIVERY_SUBMIT_FLAGS_NONE;
    options.max_topology_age_ns = UINT64_C(60000000000);
    options.target_limit = UINT32_C(1);

    solana_delivery_request_id_t request_id =
        SOLANA_DELIVERY_REQUEST_ID_NONE;

    if (solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id) != SOLANA_DELIVERY_STATUS_OK) {
        solana_delivery_client_destroy(client);
        return 1;
    }

    solana_delivery_event_t event = {0};
    size_t event_count = 0;

    if (solana_delivery_client_poll_events(
            client,
            &event,
            1U,
            (uint32_t)sizeof(event),
            &event_count) != SOLANA_DELIVERY_STATUS_OK) {
        solana_delivery_client_destroy(client);
        return 1;
    }

    const int valid_event =
        event_count == 1U &&
        request_id != SOLANA_DELIVERY_REQUEST_ID_NONE &&
        event.event_class == SOLANA_DELIVERY_EVENT_CLASS_REQUEST &&
        event.event_code == SOLANA_DELIVERY_REQUEST_EVENT_ACCEPTED &&
        event.request_id == request_id &&
        event.attempt_id == SOLANA_DELIVERY_ATTEMPT_ID_NONE;

    solana_delivery_client_destroy(client);
    return valid_event ? 0 : 1;
}
