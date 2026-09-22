// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/delivery.h"
#include "solana_delivery_internal.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void assert_target(
    const solana_delivery_route_target_t *target,
    uint32_t leader_index,
    uint32_t validator_index,
    uint32_t endpoint_index
) {
    assert(target->leader_index == leader_index);
    assert(target->validator_index == validator_index);
    assert(target->endpoint_index == endpoint_index);
}

static void init_candidates(
    solana_delivery_topology_candidate_t candidates[5]
) {
    candidates[0] = (solana_delivery_topology_candidate_t){
        .leader_index = UINT32_C(7),
        .validator_index = UINT32_C(1),
        .endpoint_index = UINT32_C(3),
    };
    candidates[1] = (solana_delivery_topology_candidate_t){
        .leader_index = UINT32_C(8),
        .validator_index = UINT32_C(2),
        .endpoint_index = UINT32_C(3),
    };
    candidates[2] = (solana_delivery_topology_candidate_t){
        .leader_index = UINT32_C(9),
        .validator_index = UINT32_C(1),
        .endpoint_index = UINT32_C(3),
    };
    candidates[3] = (solana_delivery_topology_candidate_t){
        .leader_index = UINT32_C(10),
        .validator_index = UINT32_C(1),
        .endpoint_index = UINT32_C(4),
    };
    candidates[4] = (solana_delivery_topology_candidate_t){
        .leader_index = UINT32_C(11),
        .validator_index = UINT32_C(2),
        .endpoint_index = UINT32_C(3),
    };
}

static void test_argument_contract(void) {
    solana_delivery_topology_candidate_t candidates[1] = {
        {
            .leader_index = UINT32_C(1),
            .validator_index = UINT32_C(2),
            .endpoint_index = UINT32_C(3),
        },
    };
    solana_delivery_route_target_t target;
    size_t selected = SIZE_MAX;
    size_t unique = SIZE_MAX;

    assert(
        solana_delivery_plan_routes(
            candidates,
            1U,
            1U,
            &target,
            1U,
            NULL,
            &unique
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    assert(
        solana_delivery_plan_routes(
            candidates,
            1U,
            1U,
            &target,
            1U,
            &selected,
            NULL
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    selected = SIZE_MAX;
    unique = SIZE_MAX;
    assert(
        solana_delivery_plan_routes(
            candidates,
            1U,
            0U,
            &target,
            1U,
            &selected,
            &unique
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(selected == 0U);
    assert(unique == 0U);

    selected = SIZE_MAX;
    unique = SIZE_MAX;
    assert(
        solana_delivery_plan_routes(
            NULL,
            1U,
            1U,
            &target,
            1U,
            &selected,
            &unique
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(selected == 0U);
    assert(unique == 0U);

    selected = SIZE_MAX;
    unique = SIZE_MAX;
    assert(
        solana_delivery_plan_routes(
            candidates,
            1U,
            1U,
            NULL,
            1U,
            &selected,
            &unique
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );
    assert(selected == 0U);
    assert(unique == 0U);
}

static void test_empty_candidate_sequence(void) {
    size_t selected = SIZE_MAX;
    size_t unique = SIZE_MAX;

    assert(
        solana_delivery_plan_routes(
            NULL,
            0U,
            1U,
            NULL,
            0U,
            &selected,
            &unique
        ) ==
        SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE
    );
    assert(selected == 0U);
    assert(unique == 0U);
}

static void test_deduplication_and_order(void) {
    solana_delivery_topology_candidate_t candidates[5];
    init_candidates(candidates);

    solana_delivery_route_target_t targets[3];
    size_t selected = 0U;
    size_t unique = 0U;

    assert(
        solana_delivery_plan_routes(
            candidates,
            5U,
            5U,
            targets,
            3U,
            &selected,
            &unique
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(selected == 3U);
    assert(unique == 3U);

    assert_target(
        &targets[0],
        UINT32_C(7),
        UINT32_C(1),
        UINT32_C(3)
    );
    assert_target(
        &targets[1],
        UINT32_C(8),
        UINT32_C(2),
        UINT32_C(3)
    );
    assert_target(
        &targets[2],
        UINT32_C(10),
        UINT32_C(1),
        UINT32_C(4)
    );
}

static void test_target_limit(void) {
    solana_delivery_topology_candidate_t candidates[5];
    init_candidates(candidates);

    solana_delivery_route_target_t targets[2];
    size_t selected = 0U;
    size_t unique = 0U;

    assert(
        solana_delivery_plan_routes(
            candidates,
            5U,
            2U,
            targets,
            2U,
            &selected,
            &unique
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(selected == 2U);
    assert(unique == 3U);

    assert_target(
        &targets[0],
        UINT32_C(7),
        UINT32_C(1),
        UINT32_C(3)
    );
    assert_target(
        &targets[1],
        UINT32_C(8),
        UINT32_C(2),
        UINT32_C(3)
    );
}

static void test_capacity_query_and_atomicity(void) {
    solana_delivery_topology_candidate_t candidates[5];
    init_candidates(candidates);

    size_t selected = 0U;
    size_t unique = 0U;

    assert(
        solana_delivery_plan_routes(
            candidates,
            5U,
            2U,
            NULL,
            0U,
            &selected,
            &unique
        ) ==
        SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED
    );
    assert(selected == 2U);
    assert(unique == 3U);

    solana_delivery_route_target_t targets[2];
    solana_delivery_route_target_t before[2];

    memset(targets, 0xA5, sizeof(targets));
    memcpy(before, targets, sizeof(before));

    assert(
        solana_delivery_plan_routes(
            candidates,
            5U,
            2U,
            targets,
            1U,
            &selected,
            &unique
        ) ==
        SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED
    );
    assert(selected == 2U);
    assert(unique == 3U);
    assert(memcmp(
               targets,
               before,
               sizeof(targets)
           ) == 0);
}

static void test_limit_one(void) {
    solana_delivery_topology_candidate_t candidates[5];
    init_candidates(candidates);

    solana_delivery_route_target_t target;
    size_t selected = 0U;
    size_t unique = 0U;

    assert(
        solana_delivery_plan_routes(
            candidates,
            5U,
            1U,
            &target,
            1U,
            &selected,
            &unique
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(selected == 1U);
    assert(unique == 3U);
    assert_target(
        &target,
        UINT32_C(7),
        UINT32_C(1),
        UINT32_C(3)
    );
}

static void test_determinism(void) {
    solana_delivery_topology_candidate_t candidates[5];
    init_candidates(candidates);

    solana_delivery_route_target_t first[3];
    solana_delivery_route_target_t second[3];
    size_t first_selected = 0U;
    size_t first_unique = 0U;
    size_t second_selected = 0U;
    size_t second_unique = 0U;

    assert(
        solana_delivery_plan_routes(
            candidates,
            5U,
            3U,
            first,
            3U,
            &first_selected,
            &first_unique
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(
        solana_delivery_plan_routes(
            candidates,
            5U,
            3U,
            second,
            3U,
            &second_selected,
            &second_unique
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(first_selected == second_selected);
    assert(first_unique == second_unique);
    assert(memcmp(first, second, sizeof(first)) == 0);
}

int main(void) {
    test_argument_contract();
    test_empty_candidate_sequence();
    test_deduplication_and_order();
    test_target_limit();
    test_capacity_query_and_atomicity();
    test_limit_one();
    test_determinism();
    return 0;
}
