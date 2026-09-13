// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/delivery.h"

#include <cstdint>
#include <type_traits>

static_assert(sizeof(solana_delivery_validator_identity_t) == 32);
static_assert(sizeof(solana_delivery_endpoint_t) == 32);
static_assert(sizeof(solana_delivery_validator_t) == 48);
static_assert(sizeof(solana_delivery_validator_endpoint_t) == 16);
static_assert(sizeof(solana_delivery_leader_t) == 32);
static_assert(sizeof(solana_delivery_submit_options_t) == 8);
static_assert(sizeof(solana_delivery_event_t) == 64);

static_assert(std::is_standard_layout<
                  solana_delivery_endpoint_t>::value);
static_assert(std::is_standard_layout<
                  solana_delivery_topology_t>::value);
static_assert(std::is_standard_layout<
                  solana_delivery_event_t>::value);

int main() {
    solana_delivery_endpoint_t endpoint{};
    endpoint.struct_size =
        static_cast<std::uint32_t>(sizeof(endpoint));

    return endpoint.struct_size == sizeof(endpoint) ? 0 : 1;
}
