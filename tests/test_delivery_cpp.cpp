// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/delivery.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

static_assert(sizeof(solana_delivery_validator_identity_t) == 32);
static_assert(sizeof(solana_delivery_endpoint_t) == 32);
static_assert(sizeof(solana_delivery_validator_t) == 48);
static_assert(sizeof(solana_delivery_validator_endpoint_t) == 16);
static_assert(sizeof(solana_delivery_leader_t) == 32);
static_assert(sizeof(solana_delivery_submit_options_t) == 24);
static_assert(sizeof(solana_delivery_event_t) == 64);
static_assert(
    SOLANA_DELIVERY_REQUEST_EVENT_ACCEPTED == UINT32_C(1)
);

using solana_delivery_poll_events_fn_t =
    solana_delivery_status_t (*)(
        solana_delivery_client_t *,
        solana_delivery_event_t *,
        std::size_t,
        std::uint32_t,
        std::size_t *
    );

static_assert(
    std::is_same<
        decltype(&solana_delivery_client_poll_events),
        solana_delivery_poll_events_fn_t
    >::value
);

static_assert(std::is_standard_layout<
                  solana_delivery_endpoint_t>::value);
static_assert(std::is_standard_layout<
                  solana_delivery_topology_t>::value);
static_assert(std::is_standard_layout<
                  solana_delivery_submit_options_t>::value);
static_assert(std::is_standard_layout<
                  solana_delivery_event_t>::value);

int main() {
    solana_delivery_endpoint_t endpoint{};
    endpoint.struct_size =
        static_cast<std::uint32_t>(sizeof(endpoint));

    solana_delivery_topology_t topology{};
    topology.struct_size =
        static_cast<std::uint32_t>(sizeof(topology));

    const solana_delivery_status_t status =
        solana_delivery_topology_validate(&topology);

    solana_delivery_client_t *client = nullptr;
    const solana_delivery_status_t create_status =
        solana_delivery_client_create(&client);

    solana_delivery_status_t install_status =
        SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;

    if (create_status == SOLANA_DELIVERY_STATUS_OK) {
        install_status =
            solana_delivery_client_install_topology(
                client,
                &topology
            );
    }

    solana_delivery_submit_options_t options{};
    options.struct_size =
        static_cast<std::uint32_t>(sizeof(options));
    options.max_topology_age_ns = UINT64_MAX;
    options.target_limit = UINT32_C(1);

    const std::uint8_t transaction = UINT8_C(1);
    solana_delivery_request_id_t request_id = UINT64_C(99);

    solana_delivery_status_t submit_status =
        SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;

    if (install_status == SOLANA_DELIVERY_STATUS_OK) {
        submit_status =
            solana_delivery_client_submit(
                client,
                &transaction,
                sizeof(transaction),
                &options,
                &request_id
            );
    }

    solana_delivery_client_destroy(client);

    return endpoint.struct_size == sizeof(endpoint) &&
                   status == SOLANA_DELIVERY_STATUS_OK &&
                   create_status == SOLANA_DELIVERY_STATUS_OK &&
                   install_status == SOLANA_DELIVERY_STATUS_OK &&
                   submit_status ==
                       SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE &&
                   request_id ==
                       SOLANA_DELIVERY_REQUEST_ID_NONE
               ? 0
               : 1;
}
