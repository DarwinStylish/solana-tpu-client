// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#ifndef SOLANA_DELIVERY_H
#define SOLANA_DELIVERY_H

#include <stddef.h>
#include <stdint.h>

#define SOLANA_DELIVERY_ABI_VERSION_MAJOR UINT32_C(1)
#define SOLANA_DELIVERY_ABI_VERSION_MINOR UINT32_C(0)
#define SOLANA_DELIVERY_ABI_VERSION UINT32_C(0x00010000)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct solana_delivery_client solana_delivery_client_t;

typedef uint32_t solana_delivery_status_t;

#define SOLANA_DELIVERY_STATUS_OK UINT32_C(0)
#define SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT UINT32_C(1)
#define SOLANA_DELIVERY_STATUS_UNSUPPORTED UINT32_C(2)
#define SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED UINT32_C(3)
#define SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE UINT32_C(4)
#define SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE UINT32_C(5)
#define SOLANA_DELIVERY_STATUS_INTERNAL_ERROR UINT32_C(6)

/*
 * API status values describe local library operations only.
 * SOLANA_DELIVERY_STATUS_OK never implies transport completion,
 * transaction landing, or confirmation.
 */

typedef uint64_t solana_delivery_request_id_t;
typedef uint64_t solana_delivery_attempt_id_t;

#define SOLANA_DELIVERY_REQUEST_ID_NONE UINT64_C(0)
#define SOLANA_DELIVERY_ATTEMPT_ID_NONE UINT64_C(0)

typedef uint8_t solana_delivery_address_family_t;

#define SOLANA_DELIVERY_ADDRESS_UNSPEC UINT8_C(0)
#define SOLANA_DELIVERY_ADDRESS_IPV4 UINT8_C(4)
#define SOLANA_DELIVERY_ADDRESS_IPV6 UINT8_C(6)

typedef uint8_t solana_delivery_transport_t;

#define SOLANA_DELIVERY_TRANSPORT_UNSPEC UINT8_C(0)
#define SOLANA_DELIVERY_TRANSPORT_QUIC UINT8_C(1)

typedef uint8_t solana_delivery_endpoint_role_t;

#define SOLANA_DELIVERY_ENDPOINT_ROLE_UNSPEC UINT8_C(0)
#define SOLANA_DELIVERY_ENDPOINT_ROLE_TPU UINT8_C(1)

typedef struct {
    uint8_t bytes[32];
} solana_delivery_validator_identity_t;

/*
 * Stable endpoint representation.
 *
 * port is expressed in host byte order.
 * IPv4 addresses use address[0..3] as the four network-order octets
 * and require address[4..15] to be zero.
 * IPv6 addresses use all sixteen network-order octets.
 * Reserved fields must be zero.
 */
typedef struct {
    uint32_t struct_size;
    solana_delivery_address_family_t address_family;
    solana_delivery_transport_t transport;
    solana_delivery_endpoint_role_t role;
    uint8_t reserved0;
    uint16_t port;
    uint16_t reserved1;
    uint8_t address[16];
    uint8_t reserved2[4];
} solana_delivery_endpoint_t;

typedef struct {
    uint32_t struct_size;
    uint32_t reserved0;
    solana_delivery_validator_identity_t identity;
    uint8_t reserved1[8];
} solana_delivery_validator_t;

/*
 * Explicit validator-to-endpoint association.
 * Multiple validators may reference one endpoint, and one validator
 * may reference multiple endpoints.
 */
typedef struct {
    uint32_t struct_size;
    uint32_t validator_index;
    uint32_t endpoint_index;
    uint32_t reserved0;
} solana_delivery_validator_endpoint_t;

typedef struct {
    uint32_t struct_size;
    uint32_t validator_index;
    uint64_t first_slot;
    uint64_t last_slot;
    uint64_t reserved0;
} solana_delivery_leader_t;

/*
 * Caller-supplied topology view.
 *
 * generation orders snapshots within one client lifetime and is not
 * a Solana slot. current_slot is caller-observed chain context and is
 * distinct from local monotonic freshness tracking.
 *
 * The pointer members are process-ABI pointers.
 * solana_delivery_client_install_topology copies the required snapshot
 * data before returning success, so caller storage may then be reused.
 *
 * Each array has an explicit byte stride. This permits an element
 * structure to grow by appending fields without requiring an older
 * consumer to assume its own sizeof(element) as the array stride.
 *
 * For a non-empty array, its stride describes the byte distance from
 * one element to the next. Structural validation will require enough
 * bytes for the ABI prefix understood by the implementation.
 */
typedef struct {
    uint32_t struct_size;
    uint32_t reserved0;
    uint64_t generation;
    uint64_t current_slot;
    const solana_delivery_validator_t *validators;
    uint32_t validator_count;
    uint32_t validator_stride;
    const solana_delivery_endpoint_t *endpoints;
    uint32_t endpoint_count;
    uint32_t endpoint_stride;
    const solana_delivery_validator_endpoint_t *validator_endpoints;
    uint32_t validator_endpoint_count;
    uint32_t validator_endpoint_stride;
    const solana_delivery_leader_t *leaders;
    uint32_t leader_count;
    uint32_t leader_stride;
} solana_delivery_topology_t;

/*
 * Validate the structural integrity of one caller-owned topology view.
 *
 * This function does not install topology, perform discovery, select
 * routes, open connections, or submit transactions.
 *
 * SOLANA_DELIVERY_STATUS_OK means only that the supplied representation
 * satisfies the currently supported structural contract.
 */
solana_delivery_status_t solana_delivery_topology_validate(
    const solana_delivery_topology_t *topology
);

/*
 * Create an empty delivery client.
 *
 * The returned handle owns all implementation state. Creation,
 * destruction, and topology installation initially require exclusive
 * access to the handle.
 */
solana_delivery_status_t solana_delivery_client_create(
    solana_delivery_client_t **out_client
);

/*
 * Destroy a delivery client and all implementation-owned state.
 * Passing NULL is permitted and has no effect.
 */
void solana_delivery_client_destroy(
    solana_delivery_client_t *client
);

/*
 * Validate and install one caller-supplied topology snapshot.
 *
 * On success the implementation owns an internal copy and retains no
 * borrowed caller array pointers. After the first successful install,
 * generation must increase strictly. Equal or lower generations return
 * SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE.
 *
 * Failure leaves the previously installed snapshot unchanged.
 */
solana_delivery_status_t solana_delivery_client_install_topology(
    solana_delivery_client_t *client,
    const solana_delivery_topology_t *topology
);

/*
 * Phase 1 submission options.
 *
 * flags must currently be SOLANA_DELIVERY_SUBMIT_FLAGS_NONE.
 * max_topology_age_ns and target_limit must both be nonzero.
 * reserved0 must be zero.
 *
 * Additional fields may be appended in compatible ABI revisions.
 */
typedef struct {
    uint32_t struct_size;
    uint32_t flags;
    uint64_t max_topology_age_ns;
    uint32_t target_limit;
    uint32_t reserved0;
} solana_delivery_submit_options_t;

#define SOLANA_DELIVERY_SUBMIT_FLAGS_NONE UINT32_C(0)

/*
 * Accept one already-signed opaque serialized transaction into the local
 * delivery lifecycle.
 *
 * transaction_bytes remains caller-owned. A successful call internalizes
 * all data required after return and assigns a nonzero request identifier.
 *
 * SOLANA_DELIVERY_STATUS_OK means local request acceptance only. It does
 * not imply transport progress, validator receipt, landing, or confirmation.
 *
 * Callable submission requires options->struct_size to cover the complete
 * currently required prefix through reserved0.
 *
 * On failure, no request is accepted and out_request_id remains
 * SOLANA_DELIVERY_REQUEST_ID_NONE when out_request_id itself is valid.
 *
 * Phase 1 defines no concurrent use of one client for topology installation,
 * submission, polling, or destruction.
 */
solana_delivery_status_t solana_delivery_client_submit(
    solana_delivery_client_t *client,
    const uint8_t *transaction_bytes,
    size_t transaction_length,
    const solana_delivery_submit_options_t *options,
    solana_delivery_request_id_t *out_request_id
);

typedef uint32_t solana_delivery_event_class_t;

#define SOLANA_DELIVERY_EVENT_CLASS_UNSPEC UINT32_C(0)
#define SOLANA_DELIVERY_EVENT_CLASS_REQUEST UINT32_C(1)
#define SOLANA_DELIVERY_EVENT_CLASS_ATTEMPT UINT32_C(2)
#define SOLANA_DELIVERY_EVENT_CLASS_OBSERVATION UINT32_C(3)

/*
 * Structural event envelope only.
 *
 * event_code values are intentionally not frozen yet; they must be
 * derived from implemented behavior before callable polling APIs are
 * introduced. attempt_id is SOLANA_DELIVERY_ATTEMPT_ID_NONE when an
 * event is not associated with a transport attempt.
 *
 * request_sequence is library-local ordering within one request.
 * monotonic_time_ns belongs to the library monotonic clock domain.
 */
typedef struct {
    uint32_t struct_size;
    solana_delivery_event_class_t event_class;
    uint32_t event_code;
    int32_t diagnostic_code;
    solana_delivery_request_id_t request_id;
    solana_delivery_attempt_id_t attempt_id;
    uint64_t request_sequence;
    uint64_t monotonic_time_ns;
    uint64_t reserved[2];
} solana_delivery_event_t;

#ifdef __cplusplus
}
#endif

#endif
