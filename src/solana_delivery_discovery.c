// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/discovery.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DISCOVERY_PROVIDER_MIN_SIZE \
    (offsetof(solana_delivery_discovery_provider_t, release) + \
     sizeof(((solana_delivery_discovery_provider_t *)0)->release))

static bool pointer_is_aligned_for(
    const void *pointer,
    size_t alignment
) {
    return pointer != NULL &&
           alignment != 0U &&
           ((uintptr_t)pointer % alignment) == 0U;
}

static solana_delivery_status_t validate_provider(
    solana_delivery_client_t *client,
    const solana_delivery_discovery_provider_t *provider
) {
    if (client == NULL ||
        provider == NULL ||
        !pointer_is_aligned_for(
            provider,
            _Alignof(solana_delivery_discovery_provider_t)
        )) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if ((size_t)provider->struct_size <
        DISCOVERY_PROVIDER_MIN_SIZE) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if (provider->flags !=
        SOLANA_DELIVERY_DISCOVERY_PROVIDER_FLAGS_NONE) {
        return SOLANA_DELIVERY_STATUS_UNSUPPORTED;
    }

    if (provider->acquire == NULL) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    return SOLANA_DELIVERY_STATUS_OK;
}

solana_delivery_status_t
solana_delivery_client_refresh_topology(
    solana_delivery_client_t *client,
    const solana_delivery_discovery_provider_t *provider
) {
    solana_delivery_status_t status =
        validate_provider(client, provider);

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        return status;
    }

    void *context = provider->context;
    solana_delivery_discovery_acquire_fn acquire =
        provider->acquire;
    solana_delivery_discovery_release_fn release =
        provider->release;

    const solana_delivery_topology_t *topology = NULL;

    status = acquire(context, &topology);

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        return status;
    }

    if (topology == NULL) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    status = solana_delivery_client_install_topology(
        client,
        topology
    );

    if (release != NULL) {
        release(context, topology);
    }

    return status;
}
