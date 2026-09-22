// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#define _POSIX_C_SOURCE 200809L

#include "solana_delivery_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STRUCT_MIN_SIZE(type, field) \
    (offsetof(type, field) + sizeof(((type *)0)->field))

static void *system_calloc(
    void *context,
    size_t count,
    size_t size
) {
    (void)context;
    return calloc(count, size);
}

static void system_free(
    void *context,
    void *pointer
) {
    (void)context;
    free(pointer);
}

static const solana_delivery_allocator_t SYSTEM_ALLOCATOR = {
    .calloc_fn = system_calloc,
    .free_fn = system_free,
    .context = NULL,
};

static bool allocator_is_valid(
    const solana_delivery_allocator_t *allocator
) {
    return allocator != NULL &&
           allocator->calloc_fn != NULL &&
           allocator->free_fn != NULL;
}

static bool bytes_are_zero(
    const uint8_t *bytes,
    size_t length
) {
    for (size_t i = 0U; i < length; ++i) {
        if (bytes[i] != 0U) {
            return false;
        }
    }

    return true;
}

static bool pointer_is_aligned(
    const void *pointer,
    size_t alignment
) {
    return alignment != 0U &&
           ((uintptr_t)pointer % alignment) == 0U;
}

static bool array_layout_is_valid(
    const void *base,
    uint32_t count,
    uint32_t stride,
    size_t minimum_element_size,
    size_t element_alignment
) {
    if (count == 0U) {
        return true;
    }

    if (base == NULL ||
        stride < minimum_element_size ||
        !pointer_is_aligned(base, element_alignment) ||
        ((size_t)stride % element_alignment) != 0U) {
        return false;
    }

    size_t final_index = (size_t)count - 1U;
    size_t stride_size = (size_t)stride;

    if (final_index >
        (SIZE_MAX - minimum_element_size) / stride_size) {
        return false;
    }

    return true;
}

static const void *array_element(
    const void *base,
    uint32_t index,
    uint32_t stride
) {
    return (const uint8_t *)base +
           ((size_t)index * (size_t)stride);
}

static bool element_size_is_valid(
    uint32_t struct_size,
    uint32_t stride,
    size_t minimum_size
) {
    return (size_t)struct_size >= minimum_size &&
           struct_size <= stride;
}

static solana_delivery_status_t validate_validator(
    const solana_delivery_validator_t *validator,
    uint32_t stride
) {
    const size_t minimum_size = STRUCT_MIN_SIZE(
        solana_delivery_validator_t,
        reserved1
    );

    if (!element_size_is_valid(
            validator->struct_size,
            stride,
            minimum_size
        ) ||
        validator->reserved0 != 0U ||
        !bytes_are_zero(
            validator->reserved1,
            sizeof(validator->reserved1)
        )) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    return SOLANA_DELIVERY_STATUS_OK;
}

static solana_delivery_status_t validate_endpoint(
    const solana_delivery_endpoint_t *endpoint,
    uint32_t stride
) {
    const size_t minimum_size = STRUCT_MIN_SIZE(
        solana_delivery_endpoint_t,
        reserved2
    );

    if (!element_size_is_valid(
            endpoint->struct_size,
            stride,
            minimum_size
        ) ||
        endpoint->reserved0 != 0U ||
        endpoint->reserved1 != 0U ||
        !bytes_are_zero(
            endpoint->reserved2,
            sizeof(endpoint->reserved2)
        ) ||
        endpoint->port == 0U) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if (endpoint->address_family ==
        SOLANA_DELIVERY_ADDRESS_UNSPEC) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if (endpoint->address_family !=
            SOLANA_DELIVERY_ADDRESS_IPV4 &&
        endpoint->address_family !=
            SOLANA_DELIVERY_ADDRESS_IPV6) {
        return SOLANA_DELIVERY_STATUS_UNSUPPORTED;
    }

    if (endpoint->transport ==
        SOLANA_DELIVERY_TRANSPORT_UNSPEC) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if (endpoint->transport !=
        SOLANA_DELIVERY_TRANSPORT_QUIC) {
        return SOLANA_DELIVERY_STATUS_UNSUPPORTED;
    }

    if (endpoint->role ==
        SOLANA_DELIVERY_ENDPOINT_ROLE_UNSPEC) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if (endpoint->role !=
        SOLANA_DELIVERY_ENDPOINT_ROLE_TPU) {
        return SOLANA_DELIVERY_STATUS_UNSUPPORTED;
    }

    if (endpoint->address_family ==
            SOLANA_DELIVERY_ADDRESS_IPV4 &&
        !bytes_are_zero(
            endpoint->address + 4,
            sizeof(endpoint->address) - 4U
        )) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    return SOLANA_DELIVERY_STATUS_OK;
}

static solana_delivery_status_t validate_association(
    const solana_delivery_validator_endpoint_t *association,
    uint32_t stride,
    uint32_t validator_count,
    uint32_t endpoint_count
) {
    const size_t minimum_size = STRUCT_MIN_SIZE(
        solana_delivery_validator_endpoint_t,
        reserved0
    );

    if (!element_size_is_valid(
            association->struct_size,
            stride,
            minimum_size
        ) ||
        association->reserved0 != 0U ||
        association->validator_index >= validator_count ||
        association->endpoint_index >= endpoint_count) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    return SOLANA_DELIVERY_STATUS_OK;
}

static solana_delivery_status_t validate_leader(
    const solana_delivery_leader_t *leader,
    uint32_t stride,
    uint32_t validator_count
) {
    const size_t minimum_size = STRUCT_MIN_SIZE(
        solana_delivery_leader_t,
        reserved0
    );

    if (!element_size_is_valid(
            leader->struct_size,
            stride,
            minimum_size
        ) ||
        leader->reserved0 != 0U ||
        leader->validator_index >= validator_count ||
        leader->first_slot > leader->last_slot) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    return SOLANA_DELIVERY_STATUS_OK;
}

solana_delivery_status_t solana_delivery_topology_validate(
    const solana_delivery_topology_t *topology
) {
    const size_t topology_minimum_size = STRUCT_MIN_SIZE(
        solana_delivery_topology_t,
        leader_stride
    );

    if (topology == NULL ||
        !pointer_is_aligned(
            topology,
            _Alignof(solana_delivery_topology_t)
        )) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if ((size_t)topology->struct_size < topology_minimum_size ||
        topology->reserved0 != 0U) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if (!array_layout_is_valid(
            topology->validators,
            topology->validator_count,
            topology->validator_stride,
            STRUCT_MIN_SIZE(
                solana_delivery_validator_t,
                reserved1
            ),
            _Alignof(solana_delivery_validator_t)
        ) ||
        !array_layout_is_valid(
            topology->endpoints,
            topology->endpoint_count,
            topology->endpoint_stride,
            STRUCT_MIN_SIZE(
                solana_delivery_endpoint_t,
                reserved2
            ),
            _Alignof(solana_delivery_endpoint_t)
        ) ||
        !array_layout_is_valid(
            topology->validator_endpoints,
            topology->validator_endpoint_count,
            topology->validator_endpoint_stride,
            STRUCT_MIN_SIZE(
                solana_delivery_validator_endpoint_t,
                reserved0
            ),
            _Alignof(solana_delivery_validator_endpoint_t)
        ) ||
        !array_layout_is_valid(
            topology->leaders,
            topology->leader_count,
            topology->leader_stride,
            STRUCT_MIN_SIZE(
                solana_delivery_leader_t,
                reserved0
            ),
            _Alignof(solana_delivery_leader_t)
        )) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    for (uint32_t i = 0U;
         i < topology->validator_count;
         ++i) {
        const solana_delivery_validator_t *validator =
            array_element(
                topology->validators,
                i,
                topology->validator_stride
            );

        solana_delivery_status_t status = validate_validator(
            validator,
            topology->validator_stride
        );

        if (status != SOLANA_DELIVERY_STATUS_OK) {
            return status;
        }
    }

    for (uint32_t i = 0U;
         i < topology->endpoint_count;
         ++i) {
        const solana_delivery_endpoint_t *endpoint =
            array_element(
                topology->endpoints,
                i,
                topology->endpoint_stride
            );

        solana_delivery_status_t status = validate_endpoint(
            endpoint,
            topology->endpoint_stride
        );

        if (status != SOLANA_DELIVERY_STATUS_OK) {
            return status;
        }
    }

    for (uint32_t i = 0U;
         i < topology->validator_endpoint_count;
         ++i) {
        const solana_delivery_validator_endpoint_t *association =
            array_element(
                topology->validator_endpoints,
                i,
                topology->validator_endpoint_stride
            );

        solana_delivery_status_t status = validate_association(
            association,
            topology->validator_endpoint_stride,
            topology->validator_count,
            topology->endpoint_count
        );

        if (status != SOLANA_DELIVERY_STATUS_OK) {
            return status;
        }
    }

    for (uint32_t i = 0U;
         i < topology->leader_count;
         ++i) {
        const solana_delivery_leader_t *leader = array_element(
            topology->leaders,
            i,
            topology->leader_stride
        );

        solana_delivery_status_t status = validate_leader(
            leader,
            topology->leader_stride,
            topology->validator_count
        );

        if (status != SOLANA_DELIVERY_STATUS_OK) {
            return status;
        }
    }

    return SOLANA_DELIVERY_STATUS_OK;
}

static void owned_topology_release(
    const solana_delivery_allocator_t *allocator,
    solana_delivery_owned_topology_t *owned
) {
    if (owned == NULL) {
        return;
    }

    if (!allocator_is_valid(allocator)) {
        return;
    }

    allocator->free_fn(
        allocator->context,
        owned->validators
    );
    allocator->free_fn(
        allocator->context,
        owned->endpoints
    );
    allocator->free_fn(
        allocator->context,
        owned->validator_endpoints
    );
    allocator->free_fn(
        allocator->context,
        owned->leaders
    );

    memset(owned, 0, sizeof(*owned));
}

static void owned_requests_release(
    const solana_delivery_allocator_t *allocator,
    solana_delivery_owned_request_t **head
) {
    if (!allocator_is_valid(allocator) || head == NULL) {
        return;
    }

    while (*head != NULL) {
        solana_delivery_owned_request_t *request = *head;
        *head = request->next;

        allocator->free_fn(
            allocator->context,
            request->transaction_bytes
        );
        allocator->free_fn(
            allocator->context,
            request->targets
        );
        allocator->free_fn(
            allocator->context,
            request
        );
    }
}

static solana_delivery_status_t clone_array(
    const solana_delivery_allocator_t *allocator,
    const void *source,
    uint32_t count,
    uint32_t source_stride,
    size_t element_size,
    void **out_array
) {
    if (!allocator_is_valid(allocator) ||
        out_array == NULL ||
        element_size == 0U) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    *out_array = NULL;

    if (count == 0U) {
        return SOLANA_DELIVERY_STATUS_OK;
    }

    if ((size_t)count > SIZE_MAX / element_size) {
        return SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED;
    }

    uint8_t *copy = allocator->calloc_fn(
        allocator->context,
        (size_t)count,
        element_size
    );
    if (copy == NULL) {
        return SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED;
    }

    for (uint32_t i = 0U; i < count; ++i) {
        memcpy(
            copy + ((size_t)i * element_size),
            array_element(source, i, source_stride),
            element_size
        );
    }

    *out_array = copy;
    return SOLANA_DELIVERY_STATUS_OK;
}

static void normalize_owned_struct_sizes(
    solana_delivery_owned_topology_t *owned
) {
    for (uint32_t i = 0U;
         i < owned->view.validator_count;
         ++i) {
        owned->validators[i].struct_size =
            (uint32_t)sizeof(owned->validators[i]);
    }

    for (uint32_t i = 0U;
         i < owned->view.endpoint_count;
         ++i) {
        owned->endpoints[i].struct_size =
            (uint32_t)sizeof(owned->endpoints[i]);
    }

    for (uint32_t i = 0U;
         i < owned->view.validator_endpoint_count;
         ++i) {
        owned->validator_endpoints[i].struct_size =
            (uint32_t)sizeof(owned->validator_endpoints[i]);
    }

    for (uint32_t i = 0U;
         i < owned->view.leader_count;
         ++i) {
        owned->leaders[i].struct_size =
            (uint32_t)sizeof(owned->leaders[i]);
    }
}

static solana_delivery_status_t build_owned_topology(
    const solana_delivery_allocator_t *allocator,
    const solana_delivery_topology_t *source,
    solana_delivery_owned_topology_t *owned
) {
    if (!allocator_is_valid(allocator) ||
        source == NULL ||
        owned == NULL) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    memset(owned, 0, sizeof(*owned));

    void *copy = NULL;

    solana_delivery_status_t status = clone_array(
        allocator,
        source->validators,
        source->validator_count,
        source->validator_stride,
        sizeof(solana_delivery_validator_t),
        &copy
    );
    if (status != SOLANA_DELIVERY_STATUS_OK) {
        goto fail;
    }
    owned->validators = copy;
    copy = NULL;

    status = clone_array(
        allocator,
        source->endpoints,
        source->endpoint_count,
        source->endpoint_stride,
        sizeof(solana_delivery_endpoint_t),
        &copy
    );
    if (status != SOLANA_DELIVERY_STATUS_OK) {
        goto fail;
    }
    owned->endpoints = copy;
    copy = NULL;

    status = clone_array(
        allocator,
        source->validator_endpoints,
        source->validator_endpoint_count,
        source->validator_endpoint_stride,
        sizeof(solana_delivery_validator_endpoint_t),
        &copy
    );
    if (status != SOLANA_DELIVERY_STATUS_OK) {
        goto fail;
    }
    owned->validator_endpoints = copy;
    copy = NULL;

    status = clone_array(
        allocator,
        source->leaders,
        source->leader_count,
        source->leader_stride,
        sizeof(solana_delivery_leader_t),
        &copy
    );
    if (status != SOLANA_DELIVERY_STATUS_OK) {
        goto fail;
    }
    owned->leaders = copy;

    owned->view.struct_size =
        (uint32_t)sizeof(owned->view);
    owned->view.generation = source->generation;
    owned->view.current_slot = source->current_slot;

    owned->view.validators = owned->validators;
    owned->view.validator_count = source->validator_count;
    owned->view.validator_stride =
        (uint32_t)sizeof(solana_delivery_validator_t);

    owned->view.endpoints = owned->endpoints;
    owned->view.endpoint_count = source->endpoint_count;
    owned->view.endpoint_stride =
        (uint32_t)sizeof(solana_delivery_endpoint_t);

    owned->view.validator_endpoints =
        owned->validator_endpoints;
    owned->view.validator_endpoint_count =
        source->validator_endpoint_count;
    owned->view.validator_endpoint_stride =
        (uint32_t)sizeof(
            solana_delivery_validator_endpoint_t
        );

    owned->view.leaders = owned->leaders;
    owned->view.leader_count = source->leader_count;
    owned->view.leader_stride =
        (uint32_t)sizeof(solana_delivery_leader_t);

    normalize_owned_struct_sizes(owned);

    return SOLANA_DELIVERY_STATUS_OK;

fail:
    owned_topology_release(allocator, owned);
    return status;
}

static solana_delivery_status_t
system_monotonic_time_ns(
    void *context,
    uint64_t *out_time_ns
) {
    (void)context;

    if (out_time_ns == NULL) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 ||
        now.tv_sec < 0 ||
        now.tv_nsec < 0 ||
        now.tv_nsec >= 1000000000L) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    uint64_t seconds = (uint64_t)now.tv_sec;

    if (seconds > UINT64_MAX / UINT64_C(1000000000)) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    *out_time_ns =
        seconds * UINT64_C(1000000000) +
        (uint64_t)now.tv_nsec;

    return SOLANA_DELIVERY_STATUS_OK;
}

solana_delivery_status_t
solana_delivery_client_create_with_dependencies(
    const solana_delivery_allocator_t *allocator,
    solana_delivery_monotonic_time_fn monotonic_time_fn,
    void *monotonic_time_context,
    solana_delivery_client_t **out_client
) {
    if (!allocator_is_valid(allocator) ||
        monotonic_time_fn == NULL ||
        out_client == NULL) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    *out_client = NULL;

    solana_delivery_client_t *client =
        allocator->calloc_fn(
            allocator->context,
            1U,
            sizeof(*client)
        );

    if (client == NULL) {
        return SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED;
    }

    client->allocator = *allocator;
    client->monotonic_time_fn = monotonic_time_fn;
    client->monotonic_time_context =
        monotonic_time_context;
    client->next_request_id = UINT64_C(1);

    *out_client = client;
    return SOLANA_DELIVERY_STATUS_OK;
}

solana_delivery_status_t
solana_delivery_client_create_with_allocator(
    const solana_delivery_allocator_t *allocator,
    solana_delivery_client_t **out_client
) {
    return solana_delivery_client_create_with_dependencies(
        allocator,
        system_monotonic_time_ns,
        NULL,
        out_client
    );
}

solana_delivery_status_t solana_delivery_client_create(
    solana_delivery_client_t **out_client
) {
    return solana_delivery_client_create_with_allocator(
        &SYSTEM_ALLOCATOR,
        out_client
    );
}

void solana_delivery_client_destroy(
    solana_delivery_client_t *client
) {
    if (client == NULL) {
        return;
    }

    solana_delivery_allocator_t allocator =
        client->allocator;

    owned_requests_release(
        &allocator,
        &client->request_head
    );

    owned_topology_release(
        &allocator,
        &client->topology
    );

    allocator.free_fn(
        allocator.context,
        client
    );
}

solana_delivery_status_t solana_delivery_client_install_topology(
    solana_delivery_client_t *client,
    const solana_delivery_topology_t *topology
) {
    if (client == NULL) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    solana_delivery_status_t status =
        solana_delivery_topology_validate(topology);

    if (status != SOLANA_DELIVERY_STATUS_OK) {
        return status;
    }

    if (client->has_topology &&
        topology->generation <=
            client->topology.view.generation) {
        return SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE;
    }

    solana_delivery_owned_topology_t replacement;

    status = build_owned_topology(
        &client->allocator,
        topology,
        &replacement
    );
    if (status != SOLANA_DELIVERY_STATUS_OK) {
        return status;
    }

    status = client->monotonic_time_fn(
        client->monotonic_time_context,
        &replacement.received_monotonic_ns
    );
    if (status != SOLANA_DELIVERY_STATUS_OK) {
        owned_topology_release(
            &client->allocator,
            &replacement
        );
        return status;
    }

    owned_topology_release(
        &client->allocator,
        &client->topology
    );
    client->topology = replacement;
    client->has_topology = true;

    return SOLANA_DELIVERY_STATUS_OK;
}
