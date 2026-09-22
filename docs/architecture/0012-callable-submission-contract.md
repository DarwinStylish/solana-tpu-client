# 12. Callable Submission Contract

Date: 2026-09-22

## Status

Accepted as the initial callable transaction-submission contract.

The callable local-acceptance behavior described by this record is implemented. Transport attempts, terminal request lifecycle transitions, event polling, retries, and observation remain unimplemented.

This record defines local request acceptance and composition of the already-implemented topology-freshness, topology-resolution, and bounded route-planning stages.

It does not define transport attempts, retries, event polling, terminal request states, landing observation, or confirmation.

## Context

The public delivery ABI already defines an opaque client, opaque request identifiers, extensible submission options, and local-only status semantics.

The implementation already provides:

- owned topology snapshots;
- local monotonic topology-freshness evaluation;
- deterministic slot-to-candidate resolution;
- deterministic bounded route planning.

Callable submission must compose those stages without changing their individual semantics and without claiming transport progress.

A successful submission must also establish enough library-owned request state that the request remains valid after the caller reuses its transaction buffer or installs a newer topology snapshot.

## Public Submission Function

The initial callable API is:

```c
solana_delivery_status_t solana_delivery_client_submit(
    solana_delivery_client_t *client,
    const uint8_t *transaction_bytes,
    size_t transaction_length,
    const solana_delivery_submit_options_t *options,
    solana_delivery_request_id_t *out_request_id
);
```

`out_request_id` is set to `SOLANA_DELIVERY_REQUEST_ID_NONE` before request acceptance whenever the pointer itself is valid.

Only a successful locally accepted request produces a nonzero request identifier.

## Submission Options

Extend `solana_delivery_submit_options_t` append-only to:

```c
typedef struct {
    uint32_t struct_size;
    uint32_t flags;
    uint64_t max_topology_age_ns;
    uint32_t target_limit;
    uint32_t reserved0;
} solana_delivery_submit_options_t;
```

The structure therefore occupies 24 bytes under the supported ABI.

`flags` must initially equal `SOLANA_DELIVERY_SUBMIT_FLAGS_NONE`.

`reserved0` must be zero.

`max_topology_age_ns` must be greater than zero.

`target_limit` must be greater than zero.

The public `uint32_t` target limit is converted to the internal planning `size_t` only after validation.

Callable submission requires a `struct_size` large enough to include the complete currently required prefix through `reserved0`.

Larger compatible option structures are accepted, but fields beyond the known prefix are not read.

No implicit freshness or target-limit defaults are introduced.

## Transaction Buffer

`transaction_bytes` must be non-null and `transaction_length` must be greater than zero.

The bytes are treated as an opaque already-signed serialized transaction.

This layer does not parse, construct, modify, sign, or reinterpret the transaction.

The submission API does not introduce a Solana-specific fixed transaction-size constant before the corresponding transport contract exists.

Allocation failure while internalizing the caller-provided byte range returns `SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED`.

No borrowed transaction pointer may survive a successful submission call.

## Routing Slot

The initial callable submission uses `current_slot` from the installed topology snapshot as the literal slot supplied to deterministic topology resolution.

`current_slot` remains caller-observed chain context. It is not used as elapsed time and is not converted into monotonic freshness.

This choice does not introduce an assumed slot duration.

It also does not require the public caller to name a validator or transport endpoint.

Future plausible-leader-frontier policy may expand routing around this chain-context anchor without changing transaction-buffer ownership or local-acceptance semantics.

## Submission Composition

The initial synchronous submission-preparation sequence is:

1. validate the client, transaction buffer, options, and request-ID output;
2. map validated public options to the internal submission policy;
3. evaluate topology freshness;
4. resolve candidates for the installed snapshot `current_slot`;
5. construct the bounded deterministic route plan;
6. internalize the transaction bytes;
7. materialize request-owned target information;
8. allocate a nonzero request identifier;
9. commit the request to library-owned state;
10. return `SOLANA_DELIVERY_STATUS_OK`.

Failure before step 9 leaves no partially accepted request.

A request identifier is not consumed by argument rejection, stale topology, unavailable routing, planning failure, or allocation failure.

## Route Availability

Freshness success alone does not imply that submission can be accepted.

If the installed snapshot has no leader for `current_slot`, or matching leaders have no associated endpoints, submission returns `SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE`.

No request is created in that case.

## Request Identity

`SOLANA_DELIVERY_REQUEST_ID_NONE` remains reserved and is never assigned to an accepted request.

Request identifiers are generated by the client and are unique within that client lifetime.

Identifiers must not wrap and be reused.

If the identifier space is exhausted, submission returns `SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED` without accepting a new request.

No routing, slot, endpoint, transaction, or transport meaning may be inferred from identifier bits.

## Accepted Request Ownership

A locally accepted request owns all state required to preserve the meaning of that acceptance after the call returns.

At minimum it owns:

- its copied transaction bytes;
- transaction length;
- request identifier;
- topology generation used for planning;
- routing slot used for planning;
- the bounded selected target set.

The selected target set must not depend solely on indices into the replaceable current topology snapshot held by the client.

For every accepted target, the request therefore materializes the validator identity and transport endpoint required by later transport work.

Planner provenance indices may additionally be retained together with the topology generation for diagnostics, but they are not the sole ownership representation of an accepted route.

Installing a newer topology snapshot must not silently retarget an already accepted request.

## Local Acceptance

`SOLANA_DELIVERY_STATUS_OK` from `solana_delivery_client_submit` means only that:

- all synchronous admission and routing preparation succeeded;
- required caller data was internalized;
- a request identifier was assigned;
- the request entered library-owned local state.

It does not mean:

- a socket or QUIC connection exists;
- an attempt was created;
- bytes were sent;
- a validator received the transaction;
- the transaction landed;
- the transaction was confirmed.

Those facts require later transport, request-lifecycle, and observation mechanisms.

## Failure Atomicity

Submission preparation is transactional from the caller perspective.

Any temporary candidate arrays, route arrays, transaction copies, or request-target storage created before local acceptance are released when a later preparation step fails.

The client must not expose a partially initialized accepted request.

The output request identifier remains `SOLANA_DELIVERY_REQUEST_ID_NONE` on failure.

## Topology Replacement

Phase 1 continues to require exclusive access to a client for topology installation and submission.

No concurrent topology replacement during one submission call is supported or implied.

After submission returns success, later topology replacement may proceed according to the existing generation rules without altering request-owned target materialization.

## Request Storage

This record does not freeze the internal container used for accepted requests.

The implementation may use any ownership structure that preserves accepted requests until later request-lifecycle work defines reclamation and terminal-state behavior.

Container layout, linked-list structure, allocation strategy, and queue internals are not public ABI.

## Excluded Behavior

Callable submission in this phase does not:

- open network connections;
- create transport attempts;
- perform QUIC or TLS operations;
- retry failed transport work;
- expose polling;
- emit frozen event codes;
- infer transaction landing;
- infer transaction confirmation.

## Consequences

The public submission boundary becomes callable without fabricating transport semantics.

The existing admission, resolution, and planning components retain their independent contracts.

Accepted requests become independent of both caller-buffer lifetime and later topology replacement.

The next request-lifecycle work can build state transitions and event sequencing on top of a real locally accepted request rather than retrofitting ownership after the public function exists.
