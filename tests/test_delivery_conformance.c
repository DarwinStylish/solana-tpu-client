// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/delivery.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    solana_delivery_validator_t validator;
    solana_delivery_endpoint_t endpoint;
    solana_delivery_validator_endpoint_t association;
    solana_delivery_leader_t leader;
    solana_delivery_topology_t topology;
} fixture_t;

typedef struct {
    solana_delivery_event_t event;
    uint8_t tail[32];
} extended_event_slot_t;

static void init_fixture(
    fixture_t *fixture,
    uint64_t generation
) {
    memset(fixture, 0, sizeof(*fixture));

    fixture->validator.struct_size =
        (uint32_t)sizeof(fixture->validator);
    fixture->validator.identity.bytes[0] = UINT8_C(7);

    fixture->endpoint.struct_size =
        (uint32_t)sizeof(fixture->endpoint);
    fixture->endpoint.address_family =
        SOLANA_DELIVERY_ADDRESS_IPV4;
    fixture->endpoint.transport =
        SOLANA_DELIVERY_TRANSPORT_QUIC;
    fixture->endpoint.role =
        SOLANA_DELIVERY_ENDPOINT_ROLE_TPU;
    fixture->endpoint.port = UINT16_C(8003);
    fixture->endpoint.address[0] = UINT8_C(127);
    fixture->endpoint.address[3] = UINT8_C(1);

    fixture->association.struct_size =
        (uint32_t)sizeof(fixture->association);
    fixture->association.validator_index =
        UINT32_C(0);
    fixture->association.endpoint_index =
        UINT32_C(0);

    fixture->leader.struct_size =
        (uint32_t)sizeof(fixture->leader);
    fixture->leader.validator_index =
        UINT32_C(0);
    fixture->leader.first_slot = UINT64_C(100);
    fixture->leader.last_slot = UINT64_C(100);

    fixture->topology.struct_size =
        (uint32_t)sizeof(fixture->topology);
    fixture->topology.generation = generation;
    fixture->topology.current_slot = UINT64_C(100);

    fixture->topology.validators =
        &fixture->validator;
    fixture->topology.validator_count =
        UINT32_C(1);
    fixture->topology.validator_stride =
        (uint32_t)sizeof(fixture->validator);

    fixture->topology.endpoints =
        &fixture->endpoint;
    fixture->topology.endpoint_count =
        UINT32_C(1);
    fixture->topology.endpoint_stride =
        (uint32_t)sizeof(fixture->endpoint);

    fixture->topology.validator_endpoints =
        &fixture->association;
    fixture->topology.validator_endpoint_count =
        UINT32_C(1);
    fixture->topology.validator_endpoint_stride =
        (uint32_t)sizeof(fixture->association);

    fixture->topology.leaders =
        &fixture->leader;
    fixture->topology.leader_count =
        UINT32_C(1);
    fixture->topology.leader_stride =
        (uint32_t)sizeof(fixture->leader);
}

static solana_delivery_submit_options_t
valid_options(void) {
    return (solana_delivery_submit_options_t){
        .struct_size =
            (uint32_t)sizeof(
                solana_delivery_submit_options_t
            ),
        .flags = SOLANA_DELIVERY_SUBMIT_FLAGS_NONE,
        .max_topology_age_ns = UINT64_MAX,
        .target_limit = UINT32_C(1),
        .reserved0 = UINT32_C(0),
    };
}

static solana_delivery_client_t *
new_routable_client(fixture_t *fixture) {
    solana_delivery_client_t *client = NULL;

    assert(
        solana_delivery_client_create(&client) ==
        SOLANA_DELIVERY_STATUS_OK
    );
    assert(client != NULL);

    assert(
        solana_delivery_client_install_topology(
            client,
            &fixture->topology
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    return client;
}

static solana_delivery_request_id_t
submit_one(
    solana_delivery_client_t *client,
    uint8_t marker
) {
    const uint8_t transaction[] = {
        marker,
        UINT8_C(2),
        UINT8_C(3),
    };

    solana_delivery_submit_options_t options =
        valid_options();

    solana_delivery_request_id_t request_id =
        SOLANA_DELIVERY_REQUEST_ID_NONE;

    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &request_id
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(
        request_id !=
        SOLANA_DELIVERY_REQUEST_ID_NONE
    );

    return request_id;
}

static void assert_accepted_event(
    const solana_delivery_event_t *event,
    solana_delivery_request_id_t request_id
) {
    assert(event != NULL);

    assert(
        event->struct_size ==
        (uint32_t)sizeof(solana_delivery_event_t)
    );
    assert(
        event->event_class ==
        SOLANA_DELIVERY_EVENT_CLASS_REQUEST
    );
    assert(
        event->event_code ==
        SOLANA_DELIVERY_REQUEST_EVENT_ACCEPTED
    );
    assert(event->diagnostic_code == 0);
    assert(event->request_id == request_id);
    assert(
        event->attempt_id ==
        SOLANA_DELIVERY_ATTEMPT_ID_NONE
    );
    assert(event->request_sequence == UINT64_C(1));
    assert(event->reserved[0] == UINT64_C(0));
    assert(event->reserved[1] == UINT64_C(0));
}

static void test_public_submission_and_polling(void) {
    fixture_t fixture;
    init_fixture(&fixture, UINT64_C(1));

    solana_delivery_client_t *client =
        new_routable_client(&fixture);

    const solana_delivery_request_id_t first_id =
        submit_one(client, UINT8_C(11));

    const solana_delivery_request_id_t second_id =
        submit_one(client, UINT8_C(12));

    assert(first_id != second_id);

    size_t count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            NULL,
            0U,
            0U,
            &count
        ) == SOLANA_DELIVERY_STATUS_OK
    );
    assert(count == 0U);

    extended_event_slot_t slot;
    memset(&slot, 0xA5, sizeof(slot));

    count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            &slot.event,
            1U,
            (uint32_t)sizeof(slot),
            &count
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(count == 1U);
    assert_accepted_event(&slot.event, first_id);

    for (size_t index = 0U;
         index < sizeof(slot.tail);
         ++index) {
        assert(slot.tail[index] == UINT8_C(0xA5));
    }

    solana_delivery_event_t event;
    memset(&event, 0, sizeof(event));

    count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            &event,
            1U,
            (uint32_t)sizeof(event),
            &count
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(count == 1U);
    assert_accepted_event(&event, second_id);

    count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            &event,
            1U,
            (uint32_t)sizeof(event),
            &count
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(count == 0U);

    solana_delivery_client_destroy(client);
}

static void test_public_backpressure_recovery(void) {
    fixture_t fixture;
    init_fixture(&fixture, UINT64_C(10));

    solana_delivery_client_t *client =
        new_routable_client(&fixture);

    const uint8_t transaction[] = {
        UINT8_C(21),
        UINT8_C(22),
    };

    solana_delivery_submit_options_t options =
        valid_options();

    size_t accepted_count = 0U;

    solana_delivery_request_id_t first_id =
        SOLANA_DELIVERY_REQUEST_ID_NONE;

    solana_delivery_request_id_t previous_id =
        SOLANA_DELIVERY_REQUEST_ID_NONE;

    for (;;) {
        solana_delivery_request_id_t request_id =
            UINT64_C(99);

        const solana_delivery_status_t status =
            solana_delivery_client_submit(
                client,
                transaction,
                sizeof(transaction),
                &options,
                &request_id
            );

        if (status == SOLANA_DELIVERY_STATUS_OK) {
            assert(
                request_id !=
                SOLANA_DELIVERY_REQUEST_ID_NONE
            );

            if (accepted_count == 0U) {
                first_id = request_id;
            } else {
                assert(request_id != previous_id);
            }

            previous_id = request_id;
            ++accepted_count;
            continue;
        }

        assert(
            status ==
            SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED
        );

        assert(
            request_id ==
            SOLANA_DELIVERY_REQUEST_ID_NONE
        );

        break;
    }

    assert(accepted_count > 0U);
    assert(
        first_id !=
        SOLANA_DELIVERY_REQUEST_ID_NONE
    );

    solana_delivery_event_t event;
    memset(&event, 0, sizeof(event));

    size_t count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            &event,
            1U,
            (uint32_t)(sizeof(event) - 1U),
            &count
        ) ==
        SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    assert(count == 0U);

    count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            &event,
            1U,
            (uint32_t)sizeof(event),
            &count
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(count == 1U);
    assert_accepted_event(&event, first_id);

    solana_delivery_request_id_t recovery_id =
        UINT64_C(99);

    assert(
        solana_delivery_client_submit(
            client,
            transaction,
            sizeof(transaction),
            &options,
            &recovery_id
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(
        recovery_id !=
        SOLANA_DELIVERY_REQUEST_ID_NONE
    );
    assert(recovery_id != previous_id);

    size_t drained_after_recovery = 0U;
    solana_delivery_request_id_t last_id =
        SOLANA_DELIVERY_REQUEST_ID_NONE;

    for (;;) {
        count = SIZE_MAX;
        memset(&event, 0, sizeof(event));

        assert(
            solana_delivery_client_poll_events(
                client,
                &event,
                1U,
                (uint32_t)sizeof(event),
                &count
            ) == SOLANA_DELIVERY_STATUS_OK
        );

        if (count == 0U) {
            break;
        }

        assert(count == 1U);
        assert(
            event.event_class ==
            SOLANA_DELIVERY_EVENT_CLASS_REQUEST
        );
        assert(
            event.event_code ==
            SOLANA_DELIVERY_REQUEST_EVENT_ACCEPTED
        );

        last_id = event.request_id;
        ++drained_after_recovery;
    }

    /*
     * One event was drained before recovery and one was appended by
     * recovery, so the remaining event count equals the number of
     * submissions accepted before backpressure was observed.
     */
    assert(drained_after_recovery == accepted_count);
    assert(last_id == recovery_id);

    solana_delivery_client_destroy(client);
}

static void
test_destroy_with_pending_requests_and_events(void) {
    fixture_t fixture;
    init_fixture(&fixture, UINT64_C(20));

    solana_delivery_client_t *client =
        new_routable_client(&fixture);

    assert(
        submit_one(client, UINT8_C(31)) !=
        SOLANA_DELIVERY_REQUEST_ID_NONE
    );
    assert(
        submit_one(client, UINT8_C(32)) !=
        SOLANA_DELIVERY_REQUEST_ID_NONE
    );
    assert(
        submit_one(client, UINT8_C(33)) !=
        SOLANA_DELIVERY_REQUEST_ID_NONE
    );

    solana_delivery_client_destroy(client);
    solana_delivery_client_destroy(NULL);
}

int main(void) {
    test_public_submission_and_polling();
    test_public_backpressure_recovery();
    test_destroy_with_pending_requests_and_events();
    return 0;
}
