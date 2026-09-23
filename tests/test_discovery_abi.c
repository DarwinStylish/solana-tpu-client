// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/discovery.h"

#include <stddef.h>
#include <stdint.h>

_Static_assert(
    SOLANA_DELIVERY_DISCOVERY_PROVIDER_FLAGS_NONE == UINT32_C(0),
    "discovery provider flags changed"
);

_Static_assert(
    offsetof(
        solana_delivery_discovery_provider_t,
        struct_size
    ) == 0,
    "discovery provider struct_size offset changed"
);

_Static_assert(
    offsetof(
        solana_delivery_discovery_provider_t,
        flags
    ) == sizeof(uint32_t),
    "discovery provider flags offset changed"
);

_Static_assert(
    sizeof(
        ((solana_delivery_discovery_provider_t *)0)->struct_size
    ) == 4,
    "discovery provider struct_size width changed"
);

_Static_assert(
    sizeof(
        ((solana_delivery_discovery_provider_t *)0)->flags
    ) == 4,
    "discovery provider flags width changed"
);

int main(void) {
    return 0;
}
