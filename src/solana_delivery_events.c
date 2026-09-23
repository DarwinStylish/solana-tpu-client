// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana_delivery_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static bool pointer_is_aligned_for(
    const void *pointer,
    size_t alignment
) {
    return pointer != NULL &&
           alignment != 0U &&
           ((uintptr_t)pointer % alignment) == 0U;
}

bool solana_delivery_event_queue_has_capacity(
    const solana_delivery_client_t *client
) {
    return client != NULL &&
           client->event_queue.count <
               (size_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY;
}

solana_delivery_status_t solana_delivery_event_queue_push(
    solana_delivery_client_t *client,
    const solana_delivery_event_t *event
) {
    if (client == NULL || event == NULL) {
        return SOLANA_DELIVERY_STATUS_INTERNAL_ERROR;
    }

    if (!solana_delivery_event_queue_has_capacity(client)) {
        return SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED;
    }

    const size_t capacity =
        (size_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY;

    const size_t tail =
        (client->event_queue.head +
         client->event_queue.count) %
        capacity;

    client->event_queue.entries[tail] = *event;
    ++client->event_queue.count;

    return SOLANA_DELIVERY_STATUS_OK;
}

solana_delivery_status_t solana_delivery_client_poll_events(
    solana_delivery_client_t *client,
    solana_delivery_event_t *events,
    size_t event_capacity,
    uint32_t event_stride,
    size_t *out_event_count
) {
    if (!pointer_is_aligned_for(
            out_event_count,
            _Alignof(size_t)
        )) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    *out_event_count = 0U;

    if (client == NULL) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if (event_capacity == 0U) {
        return SOLANA_DELIVERY_STATUS_OK;
    }

    if (!pointer_is_aligned_for(
            events,
            _Alignof(solana_delivery_event_t)
        )) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    const size_t stride = (size_t)event_stride;
    const size_t minimum_event_size =
        sizeof(solana_delivery_event_t);
    const size_t event_alignment =
        _Alignof(solana_delivery_event_t);

    if (stride < minimum_event_size ||
        (stride % event_alignment) != 0U) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    if (event_capacity > SIZE_MAX / stride) {
        return SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT;
    }

    size_t copy_count = client->event_queue.count;

    if (copy_count > event_capacity) {
        copy_count = event_capacity;
    }

    uint8_t *destination =
        (uint8_t *)(void *)events;

    const size_t queue_capacity =
        (size_t)SOLANA_DELIVERY_EVENT_QUEUE_CAPACITY;

    for (size_t index = 0U;
         index < copy_count;
         ++index) {
        const size_t offset = index * stride;

        memcpy(
            destination + offset,
            &client->event_queue
                 .entries[client->event_queue.head],
            minimum_event_size
        );

        client->event_queue.head =
            (client->event_queue.head + 1U) %
            queue_capacity;

        --client->event_queue.count;
    }

    if (client->event_queue.count == 0U) {
        client->event_queue.head = 0U;
    }

    *out_event_count = copy_count;

    return SOLANA_DELIVERY_STATUS_OK;
}
