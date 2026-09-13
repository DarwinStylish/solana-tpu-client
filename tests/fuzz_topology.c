// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/delivery.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t offset;
} input_t;

typedef struct {
    solana_delivery_validator_t base;
    uint64_t extension;
} extended_validator_t;

typedef struct {
    solana_delivery_endpoint_t base;
    uint64_t extension;
} extended_endpoint_t;

typedef struct {
    solana_delivery_validator_endpoint_t base;
    uint64_t extension;
} extended_association_t;

typedef struct {
    solana_delivery_leader_t base;
    uint64_t extension;
} extended_leader_t;

static uint8_t take_u8(input_t *input) {
    if (input->offset >= input->size) {
        return UINT8_C(0);
    }

    return input->data[input->offset++];
}

static uint16_t take_u16(input_t *input) {
    uint16_t value = take_u8(input);
    value |= (uint16_t)take_u8(input) << 8;
    return value;
}

static uint32_t take_u32(input_t *input) {
    uint32_t value = take_u16(input);
    value |= (uint32_t)take_u16(input) << 16;
    return value;
}

static uint64_t take_u64(input_t *input) {
    uint64_t value = take_u32(input);
    value |= (uint64_t)take_u32(input) << 32;
    return value;
}

static void fill_bytes(
    input_t *input,
    uint8_t *destination,
    size_t length
) {
    for (size_t i = 0U; i < length; ++i) {
        destination[i] = take_u8(input);
    }
}

static uint32_t choose_struct_size(
    input_t *input,
    uint32_t normal_size,
    uint32_t extended_size
) {
    switch (take_u8(input) % UINT8_C(4)) {
        case 0:
            return normal_size;
        case 1:
            return extended_size;
        case 2:
            return normal_size == 0U
                       ? UINT32_C(0)
                       : normal_size - UINT32_C(1);
        default:
            return take_u32(input);
    }
}

static void initialize_validator(
    input_t *input,
    solana_delivery_validator_t *validator,
    uint32_t normal_size,
    uint32_t extended_size
) {
    memset(validator, 0, sizeof(*validator));
    validator->struct_size = choose_struct_size(
        input, normal_size, extended_size
    );
    validator->reserved0 =
        (take_u8(input) & UINT8_C(3)) == 0U
            ? take_u32(input)
            : UINT32_C(0);
    fill_bytes(
        input,
        validator->identity.bytes,
        sizeof(validator->identity.bytes)
    );
    if ((take_u8(input) & UINT8_C(7)) == 0U) {
        validator->reserved1[
            take_u8(input) % sizeof(validator->reserved1)
        ] = UINT8_C(1);
    }
}

static void initialize_endpoint(
    input_t *input,
    solana_delivery_endpoint_t *endpoint,
    uint32_t normal_size,
    uint32_t extended_size
) {
    memset(endpoint, 0, sizeof(*endpoint));
    endpoint->struct_size = choose_struct_size(
        input, normal_size, extended_size
    );

    switch (take_u8(input) % UINT8_C(4)) {
        case 0:
            endpoint->address_family =
                SOLANA_DELIVERY_ADDRESS_IPV4;
            break;
        case 1:
            endpoint->address_family =
                SOLANA_DELIVERY_ADDRESS_IPV6;
            break;
        case 2:
            endpoint->address_family =
                SOLANA_DELIVERY_ADDRESS_UNSPEC;
            break;
        default:
            endpoint->address_family = take_u8(input);
            break;
    }

    endpoint->transport =
        (take_u8(input) & UINT8_C(3)) != 0U
            ? SOLANA_DELIVERY_TRANSPORT_QUIC
            : take_u8(input);
    endpoint->role =
        (take_u8(input) & UINT8_C(3)) != 0U
            ? SOLANA_DELIVERY_ENDPOINT_ROLE_TPU
            : take_u8(input);
    endpoint->port =
        (take_u8(input) & UINT8_C(3)) != 0U
            ? (uint16_t)(UINT16_C(1) +
                         (take_u16(input) % UINT16_C(65535)))
            : UINT16_C(0);

    fill_bytes(
        input, endpoint->address, sizeof(endpoint->address)
    );

    if (endpoint->address_family ==
            SOLANA_DELIVERY_ADDRESS_IPV4 &&
        (take_u8(input) & UINT8_C(1)) != 0U) {
        memset(
            endpoint->address + 4,
            0,
            sizeof(endpoint->address) - 4U
        );
    }

    endpoint->reserved0 =
        (take_u8(input) & UINT8_C(7)) == 0U
            ? take_u8(input)
            : UINT8_C(0);
    endpoint->reserved1 =
        (take_u8(input) & UINT8_C(7)) == 0U
            ? take_u16(input)
            : UINT16_C(0);
    if ((take_u8(input) & UINT8_C(7)) == 0U) {
        endpoint->reserved2[
            take_u8(input) % sizeof(endpoint->reserved2)
        ] = UINT8_C(1);
    }
}

static void initialize_association(
    input_t *input,
    solana_delivery_validator_endpoint_t *association,
    uint32_t normal_size,
    uint32_t extended_size
) {
    memset(association, 0, sizeof(*association));
    association->struct_size = choose_struct_size(
        input, normal_size, extended_size
    );
    association->validator_index = take_u32(input) % UINT32_C(4);
    association->endpoint_index = take_u32(input) % UINT32_C(4);
    association->reserved0 =
        (take_u8(input) & UINT8_C(7)) == 0U
            ? take_u32(input)
            : UINT32_C(0);
}

static void initialize_leader(
    input_t *input,
    solana_delivery_leader_t *leader,
    uint32_t normal_size,
    uint32_t extended_size
) {
    memset(leader, 0, sizeof(*leader));
    leader->struct_size = choose_struct_size(
        input, normal_size, extended_size
    );
    leader->validator_index = take_u32(input) % UINT32_C(4);
    leader->first_slot = take_u64(input);
    leader->last_slot =
        (take_u8(input) & UINT8_C(1)) != 0U
            ? leader->first_slot +
                  (take_u16(input) % UINT16_C(32))
            : take_u64(input);
    leader->reserved0 =
        (take_u8(input) & UINT8_C(7)) == 0U
            ? take_u64(input)
            : UINT64_C(0);
}

static void set_validator_array(
    input_t *input,
    solana_delivery_topology_t *topology,
    solana_delivery_validator_t base[2],
    extended_validator_t extended[2]
) {
    topology->validator_count =
        take_u8(input) % UINT8_C(3);

    switch (take_u8(input) % UINT8_C(4)) {
        case 0:
            topology->validators = base;
            topology->validator_stride =
                (uint32_t)sizeof(base[0]);
            break;
        case 1:
            topology->validators =
                (const solana_delivery_validator_t *)extended;
            topology->validator_stride =
                (uint32_t)sizeof(extended[0]);
            break;
        case 2:
            topology->validators = base;
            topology->validator_stride =
                (uint32_t)sizeof(base[0]) - UINT32_C(1);
            break;
        default:
            topology->validators = NULL;
            topology->validator_stride =
                (uint32_t)sizeof(base[0]);
            break;
    }
}

static void set_endpoint_array(
    input_t *input,
    solana_delivery_topology_t *topology,
    solana_delivery_endpoint_t base[2],
    extended_endpoint_t extended[2]
) {
    topology->endpoint_count =
        take_u8(input) % UINT8_C(3);

    switch (take_u8(input) % UINT8_C(4)) {
        case 0:
            topology->endpoints = base;
            topology->endpoint_stride =
                (uint32_t)sizeof(base[0]);
            break;
        case 1:
            topology->endpoints =
                (const solana_delivery_endpoint_t *)extended;
            topology->endpoint_stride =
                (uint32_t)sizeof(extended[0]);
            break;
        case 2:
            topology->endpoints = base;
            topology->endpoint_stride =
                (uint32_t)sizeof(base[0]) - UINT32_C(1);
            break;
        default:
            topology->endpoints = NULL;
            topology->endpoint_stride =
                (uint32_t)sizeof(base[0]);
            break;
    }
}

static void set_association_array(
    input_t *input,
    solana_delivery_topology_t *topology,
    solana_delivery_validator_endpoint_t base[2],
    extended_association_t extended[2]
) {
    topology->validator_endpoint_count =
        take_u8(input) % UINT8_C(3);

    switch (take_u8(input) % UINT8_C(4)) {
        case 0:
            topology->validator_endpoints = base;
            topology->validator_endpoint_stride =
                (uint32_t)sizeof(base[0]);
            break;
        case 1:
            topology->validator_endpoints =
                (const solana_delivery_validator_endpoint_t *)extended;
            topology->validator_endpoint_stride =
                (uint32_t)sizeof(extended[0]);
            break;
        case 2:
            topology->validator_endpoints = base;
            topology->validator_endpoint_stride =
                (uint32_t)sizeof(base[0]) - UINT32_C(1);
            break;
        default:
            topology->validator_endpoints = NULL;
            topology->validator_endpoint_stride =
                (uint32_t)sizeof(base[0]);
            break;
    }
}

static void set_leader_array(
    input_t *input,
    solana_delivery_topology_t *topology,
    solana_delivery_leader_t base[2],
    extended_leader_t extended[2]
) {
    topology->leader_count =
        take_u8(input) % UINT8_C(3);

    switch (take_u8(input) % UINT8_C(4)) {
        case 0:
            topology->leaders = base;
            topology->leader_stride =
                (uint32_t)sizeof(base[0]);
            break;
        case 1:
            topology->leaders =
                (const solana_delivery_leader_t *)extended;
            topology->leader_stride =
                (uint32_t)sizeof(extended[0]);
            break;
        case 2:
            topology->leaders = base;
            topology->leader_stride =
                (uint32_t)sizeof(base[0]) - UINT32_C(1);
            break;
        default:
            topology->leaders = NULL;
            topology->leader_stride =
                (uint32_t)sizeof(base[0]);
            break;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    input_t input = {
        .data = data,
        .size = size,
        .offset = 0U,
    };

    solana_delivery_validator_t validators[2];
    extended_validator_t extended_validators[2];
    solana_delivery_endpoint_t endpoints[2];
    extended_endpoint_t extended_endpoints[2];
    solana_delivery_validator_endpoint_t associations[2];
    extended_association_t extended_associations[2];
    solana_delivery_leader_t leaders[2];
    extended_leader_t extended_leaders[2];

    memset(validators, 0, sizeof(validators));
    memset(extended_validators, 0, sizeof(extended_validators));
    memset(endpoints, 0, sizeof(endpoints));
    memset(extended_endpoints, 0, sizeof(extended_endpoints));
    memset(associations, 0, sizeof(associations));
    memset(extended_associations, 0, sizeof(extended_associations));
    memset(leaders, 0, sizeof(leaders));
    memset(extended_leaders, 0, sizeof(extended_leaders));

    for (size_t i = 0U; i < 2U; ++i) {
        initialize_validator(
            &input,
            &validators[i],
            (uint32_t)sizeof(validators[i]),
            (uint32_t)sizeof(extended_validators[i])
        );
        initialize_validator(
            &input,
            &extended_validators[i].base,
            (uint32_t)sizeof(validators[i]),
            (uint32_t)sizeof(extended_validators[i])
        );
        extended_validators[i].extension = take_u64(&input);

        initialize_endpoint(
            &input,
            &endpoints[i],
            (uint32_t)sizeof(endpoints[i]),
            (uint32_t)sizeof(extended_endpoints[i])
        );
        initialize_endpoint(
            &input,
            &extended_endpoints[i].base,
            (uint32_t)sizeof(endpoints[i]),
            (uint32_t)sizeof(extended_endpoints[i])
        );
        extended_endpoints[i].extension = take_u64(&input);

        initialize_association(
            &input,
            &associations[i],
            (uint32_t)sizeof(associations[i]),
            (uint32_t)sizeof(extended_associations[i])
        );
        initialize_association(
            &input,
            &extended_associations[i].base,
            (uint32_t)sizeof(associations[i]),
            (uint32_t)sizeof(extended_associations[i])
        );
        extended_associations[i].extension = take_u64(&input);

        initialize_leader(
            &input,
            &leaders[i],
            (uint32_t)sizeof(leaders[i]),
            (uint32_t)sizeof(extended_leaders[i])
        );
        initialize_leader(
            &input,
            &extended_leaders[i].base,
            (uint32_t)sizeof(leaders[i]),
            (uint32_t)sizeof(extended_leaders[i])
        );
        extended_leaders[i].extension = take_u64(&input);
    }

    solana_delivery_topology_t topology;
    memset(&topology, 0, sizeof(topology));

    topology.struct_size =
        (take_u8(&input) & UINT8_C(3)) != 0U
            ? (uint32_t)sizeof(topology)
            : take_u32(&input);
    topology.reserved0 =
        (take_u8(&input) & UINT8_C(7)) == 0U
            ? take_u32(&input)
            : UINT32_C(0);
    topology.generation = take_u64(&input);
    topology.current_slot = take_u64(&input);

    set_validator_array(
        &input,
        &topology,
        validators,
        extended_validators
    );
    set_endpoint_array(
        &input,
        &topology,
        endpoints,
        extended_endpoints
    );
    set_association_array(
        &input,
        &topology,
        associations,
        extended_associations
    );
    set_leader_array(
        &input,
        &topology,
        leaders,
        extended_leaders
    );

    solana_delivery_status_t first =
        solana_delivery_topology_validate(&topology);
    solana_delivery_status_t second =
        solana_delivery_topology_validate(&topology);

    assert(first == second);
    assert(first == SOLANA_DELIVERY_STATUS_OK ||
           first == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT ||
           first == SOLANA_DELIVERY_STATUS_UNSUPPORTED);

    return 0;
}
