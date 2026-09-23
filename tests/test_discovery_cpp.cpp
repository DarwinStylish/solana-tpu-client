// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/discovery.h"

#include <cstdint>
#include <type_traits>

using discovery_acquire_fn_t =
    solana_delivery_status_t (*)(
        void *,
        const solana_delivery_topology_t **
    );

using discovery_release_fn_t =
    void (*)(
        void *,
        const solana_delivery_topology_t *
    );

using discovery_refresh_fn_t =
    solana_delivery_status_t (*)(
        solana_delivery_client_t *,
        const solana_delivery_discovery_provider_t *
    );

static_assert(
    std::is_same<
        solana_delivery_discovery_acquire_fn,
        discovery_acquire_fn_t
    >::value
);

static_assert(
    std::is_same<
        solana_delivery_discovery_release_fn,
        discovery_release_fn_t
    >::value
);

static_assert(
    std::is_same<
        decltype(&solana_delivery_client_refresh_topology),
        discovery_refresh_fn_t
    >::value
);

static_assert(
    std::is_standard_layout<
        solana_delivery_discovery_provider_t
    >::value
);

static_assert(
    SOLANA_DELIVERY_DISCOVERY_PROVIDER_FLAGS_NONE ==
        UINT32_C(0)
);

int main() {
    solana_delivery_discovery_provider_t provider{};
    provider.struct_size =
        static_cast<std::uint32_t>(sizeof(provider));

    return provider.struct_size == sizeof(provider)
               ? 0
               : 1;
}
