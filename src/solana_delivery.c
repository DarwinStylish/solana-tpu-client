// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/delivery.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STRUCT_MIN_SIZE(type, field) \
    (offsetof(type, field) + sizeof(((type *)0)->field))

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
