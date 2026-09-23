// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/delivery.h"
#include "solana_delivery_internal.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    solana_delivery_event_t event;
    uint8_t tail[16];
} extended_event_slot_t;

static solana_delivery_event_t make_event(
    solana_delivery_request_id_t request_id
) {
    return (solana_delivery_event_t){
        .struct_size =
            (uint32_t)sizeof(solana_delivery_event_t),
        .event_class =
            SOLANA_DELIVERY_EVENT_CLASS_REQUEST,
        .event_code =
            SOLANA_DELIVERY_REQUEST_EVENT_ACCEPTED,
        .diagnostic_code = 0,
        .request_id = request_id,
        .attempt_id =
            SOLANA_DELIVERY_ATTEMPT_ID_NONE,
        .request_sequence = UINT64_C(1),
        .monotonic_time_ns = request_id,
        .reserved = {UINT64_C(0), UINT64_C(0)},
    };
}

static solana_delivery_client_t *new_client(void) {
    solana_delivery_client_t *client = NULL;

    assert(
        solana_delivery_client_create(&client) ==
        SOLANA_DELIVERY_STATUS_OK
    );

    assert(client != NULL);
    assert(client->event_queue.head == 0U);
    assert(client->event_queue.count == 0U);

    return client;
}

static void push_event(
    solana_delivery_client_t *client,
    solana_delivery_request_id_t request_id
) {
    const solana_delivery_event_t event =
        make_event(request_id);

    assert(
        solana_delivery_event_queue_push(
            client,
            &event
        ) == SOLANA_DELIVERY_STATUS_OK
    );
}

static void test_empty_and_zero_capacity_poll(void) {
    solana_delivery_client_t *client = new_client();

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
    assert(client->event_queue.count == 0U);

    solana_delivery_event_t event;
    memset(&event, 0xA5, sizeof(event));

    count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            &event,
            0U,
            UINT32_C(1),
            &count
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(count == 0U);
    assert(client->event_queue.count == 0U);

    solana_delivery_client_destroy(client);
}

static void test_invalid_poll_is_nondestructive(void) {
    solana_delivery_client_t *client = new_client();

    push_event(client, UINT64_C(7));

    assert(client->event_queue.count == 1U);

    solana_delivery_event_t event = {0};
    size_t count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            NULL,
            &event,
            1U,
            (uint32_t)sizeof(event),
            &count
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    assert(count == 0U);
    assert(client->event_queue.count == 1U);

    assert(
        solana_delivery_client_poll_events(
            client,
            &event,
            1U,
            (uint32_t)sizeof(event),
            NULL
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    assert(client->event_queue.count == 1U);

    count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            NULL,
            1U,
            (uint32_t)sizeof(event),
            &count
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    assert(count == 0U);
    assert(client->event_queue.count == 1U);

    _Alignas(solana_delivery_event_t)
    uint8_t event_storage[
        sizeof(solana_delivery_event_t) + 1U
    ];

    solana_delivery_event_t *misaligned_event =
        (solana_delivery_event_t *)
            (void *)(event_storage + 1U);

    count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            misaligned_event,
            1U,
            (uint32_t)sizeof(event),
            &count
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    assert(count == 0U);
    assert(client->event_queue.count == 1U);

    _Alignas(size_t)
    uint8_t count_storage[sizeof(size_t) + 1U];

    size_t *misaligned_count =
        (size_t *)(void *)(count_storage + 1U);

    assert(
        solana_delivery_client_poll_events(
            client,
            &event,
            1U,
            (uint32_t)sizeof(event),
            misaligned_count
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    assert(client->event_queue.count == 1U);

    count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            &event,
            1U,
            (uint32_t)(sizeof(event) - 1U),
            &count
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    assert(count == 0U);
    assert(client->event_queue.count == 1U);

    count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            &event,
            1U,
            (uint32_t)(sizeof(event) + 1U),
            &count
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    assert(count == 0U);
    assert(client->event_queue.count == 1U);

    const size_t overflow_capacity =
        SIZE_MAX / sizeof(event) + 1U;

    count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            &event,
            overflow_capacity,
            (uint32_t)sizeof(event),
            &count
        ) == SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT
    );

    assert(count == 0U);
    assert(client->event_queue.count == 1U);

    solana_delivery_client_destroy(client);
}

static void test_fifo_partial_and_extended_stride(void) {
    solana_delivery_client_t *client = new_client();

    push_event(client, UINT64_C(1));
    push_event(client, UINT64_C(2));
    push_event(client, UINT64_C(3));

    solana_delivery_event_t events[2];
    memset(events, 0, sizeof(events));

    size_t count = 0U;

    assert(
        solana_delivery_client_poll_events(
            client,
            events,
            2U,
            (uint32_t)sizeof(events[0]),
            &count
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(count == 2U);
    assert(events[0].request_id == UINT64_C(1));
    assert(events[1].request_id == UINT64_C(2));
    assert(events[0].struct_size ==
           (uint32_t)sizeof(solana_delivery_event_t));
    assert(events[1].struct_size ==
           (uint32_t)sizeof(solana_delivery_event_t));
    assert(client->event_queue.count == 1U);

    extended_event_slot_t slot;
    memset(&slot, 0xA5, sizeof(slot));

    count = 0U;

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
    assert(slot.event.request_id == UINT64_C(3));
    assert(slot.event.struct_size ==
           (uint32_t)sizeof(solana_delivery_event_t));

    for (size_t index = 0U;
         index < sizeof(slot.tail);
         ++index) {
        assert(slot.tail[index] == UINT8_C(0xA5));
    }

    assert(client->event_queue.count == 0U);
    assert(client->event_queue.head == 0U);

    count = SIZE_MAX;

    assert(
        solana_delivery_client_poll_events(
            client,
            events,
            2U,
            (uint32_t)sizeof(events[0]),
            &count
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(count == 0U);

    solana_delivery_client_destroy(client);
}

static void test_ring_wraparound(void) {
    solana_delivery_client_t *client = new_client();

    for (uint64_t request_id = UINT64_C(1);
         request_id <=
             (uint64_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY;
         ++request_id) {
        push_event(client, request_id);
    }

    assert(
        client->event_queue.count ==
        (size_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY
    );

    assert(
        !solana_delivery_event_queue_has_capacity(client)
    );

    solana_delivery_event_t events[
        SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY
    ];

    size_t count = 0U;

    assert(
        solana_delivery_client_poll_events(
            client,
            events,
            (size_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY -
                1U,
            (uint32_t)sizeof(events[0]),
            &count
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(
        count ==
        (size_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY -
            1U
    );

    for (size_t index = 0U;
         index < count;
         ++index) {
        assert(
            events[index].request_id ==
            (solana_delivery_request_id_t)(index + 1U)
        );
    }

    assert(client->event_queue.count == 1U);

    for (uint64_t request_id =
             (uint64_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY +
             UINT64_C(1);
         request_id <=
             (uint64_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY *
                 UINT64_C(2) -
             UINT64_C(1);
         ++request_id) {
        push_event(client, request_id);
    }

    assert(
        client->event_queue.count ==
        (size_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY
    );

    count = 0U;

    assert(
        solana_delivery_client_poll_events(
            client,
            events,
            (size_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY,
            (uint32_t)sizeof(events[0]),
            &count
        ) == SOLANA_DELIVERY_STATUS_OK
    );

    assert(
        count ==
        (size_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY
    );

    assert(
        events[0].request_id ==
        (solana_delivery_request_id_t)
            SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY
    );

    for (size_t index = 1U;
         index < count;
         ++index) {
        assert(
            events[index].request_id ==
            (solana_delivery_request_id_t)(
                (size_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY +
                index
            )
        );
    }

    assert(client->event_queue.count == 0U);
    assert(client->event_queue.head == 0U);

    solana_delivery_client_destroy(client);
}

int main(void) {
    test_empty_and_zero_capacity_poll();
    test_invalid_poll_is_nondestructive();
    test_fifo_partial_and_extended_stride();
    test_ring_wraparound();
    return 0;
}
