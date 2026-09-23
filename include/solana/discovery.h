// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#ifndef SOLANA_DISCOVERY_H
#define SOLANA_DISCOVERY_H

#include "solana/delivery.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Acquire one coherent caller-owned topology snapshot.
 *
 * On SOLANA_DELIVERY_STATUS_OK, *out_topology must identify a complete
 * borrowed topology view that remains valid until the matching optional
 * release callback returns.
 *
 * On failure, no borrow is established.
 */
typedef solana_delivery_status_t
(*solana_delivery_discovery_acquire_fn)(
    void *context,
    const solana_delivery_topology_t **out_topology
);

/*
 * Release one topology borrow previously returned successfully by acquire.
 *
 * Providers whose snapshot storage requires no per-acquisition cleanup may
 * leave the release callback NULL.
 */
typedef void
(*solana_delivery_discovery_release_fn)(
    void *context,
    const solana_delivery_topology_t *topology
);

#define SOLANA_DELIVERY_DISCOVERY_PROVIDER_FLAGS_NONE UINT32_C(0)

/*
 * Caller-owned discovery provider descriptor.
 *
 * context is opaque caller-owned state and may be NULL.
 * acquire is required.
 * release is optional.
 *
 * The descriptor and context are borrowed only for the duration of one
 * explicit topology refresh call and are not retained by the client.
 *
 * Additional fields may be appended in compatible ABI revisions.
 */
typedef struct {
    uint32_t struct_size;
    uint32_t flags;
    void *context;
    solana_delivery_discovery_acquire_fn acquire;
    solana_delivery_discovery_release_fn release;
} solana_delivery_discovery_provider_t;

/*
 * Acquire and install one topology snapshot through a caller-owned provider.
 *
 * Refresh is explicit and synchronous. It does not occur implicitly during
 * transaction submission.
 *
 * Successful acquisition is passed through the existing topology-installation
 * contract, including structural validation, generation ordering,
 * copy-on-install ownership, and monotonic receipt-time tracking.
 *
 * The provider descriptor requires struct_size to cover the complete initial
 * prefix through release. flags must currently be
 * SOLANA_DELIVERY_DISCOVERY_PROVIDER_FLAGS_NONE.
 *
 * If acquire returns a non-OK status, no borrow is established and release
 * is not invoked.
 *
 * If acquire returns SOLANA_DELIVERY_STATUS_OK with a NULL topology pointer,
 * refresh returns SOLANA_DELIVERY_STATUS_INTERNAL_ERROR and release is not
 * invoked because no valid borrow was established.
 *
 * For a successful acquisition of a non-NULL topology, a non-NULL release
 * callback is invoked exactly once after the installation attempt, regardless
 * of whether installation succeeds.
 *
 * Phase 1 requires exclusive access to the client during refresh.
 */
solana_delivery_status_t
solana_delivery_client_refresh_topology(
    solana_delivery_client_t *client,
    const solana_delivery_discovery_provider_t *provider
);

#ifdef __cplusplus
}
#endif

#endif
