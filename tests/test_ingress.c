// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/ingress.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define TEST_PORT 49000

_Static_assert(sizeof(solana_trade_side_t) == 1,
               "solana_trade_side_t ABI changed");
_Static_assert(sizeof(solana_ingress_event_t) == SOLANA_INGRESS_EVENT_SIZE,
               "solana_ingress_event_t ABI changed");
_Static_assert(offsetof(solana_ingress_event_t, instruction_type) == 0,
               "instruction_type ABI changed");
_Static_assert(offsetof(solana_ingress_event_t, source_timestamp_ms) == 8,
               "source_timestamp_ms ABI changed");
_Static_assert(offsetof(solana_ingress_event_t, price_raw) == 16,
               "price_raw ABI changed");
_Static_assert(offsetof(solana_ingress_event_t, quantity_raw) == 24,
               "quantity_raw ABI changed");
_Static_assert(offsetof(solana_ingress_event_t, ingress_sequence_id) == 32,
               "ingress_sequence_id ABI changed");
_Static_assert(offsetof(solana_ingress_event_t, side) == 40,
               "side ABI changed");


static atomic_bool running = ATOMIC_VAR_INIT(true);
static atomic_bool received = ATOMIC_VAR_INIT(false);
static solana_ingress_event_t received_event;
static int ingress_result = -1;

static void store_u64_le(uint8_t *p, uint64_t value) {
    for (unsigned int i = 0; i < 8; ++i) {
        p[i] = (uint8_t)(value >> (i * 8));
    }
}

static void on_event(
    const solana_ingress_event_t *event,
    void *context
) {
    solana_ingress_event_t *destination = context;
    *destination = *event;
    atomic_store_explicit(&received, true, memory_order_release);
}

static int should_continue(void *context) {
    atomic_bool *flag = context;
    return atomic_load_explicit(flag, memory_order_acquire) ? 1 : 0;
}

static void *ingress_thread(void *arg) {
    (void)arg;
    ingress_result = solana_ingress_run_local(
        TEST_PORT,
        on_event,
        &received_event,
        should_continue,
        &running
    );
    return NULL;
}

int main(void) {
    uint8_t packet[SOLANA_INGRESS_WIRE_SIZE] = {0};
    store_u64_le(packet + 0, 1);
    store_u64_le(packet + 8, 1690000000000ULL);
    store_u64_le(packet + 16, 15000000000ULL);
    store_u64_le(packet + 24, 100000000ULL);
    packet[32] = SOLANA_TRADE_SIDE_BUY;

    solana_ingress_event_t decoded;

    assert(!solana_ingress_decode(NULL, sizeof(packet), 1, &decoded));
    assert(!solana_ingress_decode(packet, sizeof(packet), 1, NULL));
    assert(!solana_ingress_decode(packet, 32, 1, &decoded));

    uint8_t oversized[SOLANA_INGRESS_WIRE_SIZE + 1] = {0};
    memcpy(oversized, packet, sizeof(packet));
    assert(!solana_ingress_decode(
        oversized, sizeof(oversized), 1, &decoded
    ));

    packet[32] = 2;
    assert(!solana_ingress_decode(
        packet, sizeof(packet), 1, &decoded
    ));
    packet[32] = SOLANA_TRADE_SIDE_BUY;

    assert(solana_ingress_decode(
        packet, sizeof(packet), 7, &decoded
    ));
    assert(decoded.instruction_type == 1);
    assert(decoded.source_timestamp_ms == 1690000000000ULL);
    assert(decoded.price_raw == 15000000000ULL);
    assert(decoded.quantity_raw == 100000000ULL);
    assert(decoded.ingress_sequence_id == 7);
    assert(decoded.side == SOLANA_TRADE_SIDE_BUY);
    for (size_t i = 0; i < sizeof(decoded.reserved); ++i) {
        assert(decoded.reserved[i] == 0);
    }

    packet[32] = SOLANA_TRADE_SIDE_SELL;
    assert(solana_ingress_decode(
        packet, sizeof(packet), 8, &decoded
    ));
    assert(decoded.side == SOLANA_TRADE_SIDE_SELL);
    packet[32] = SOLANA_TRADE_SIDE_BUY;

    errno = 0;
    assert(solana_ingress_run_local(
        TEST_PORT, NULL, NULL, should_continue, &running
    ) == -1);
    assert(errno == EINVAL);

    errno = 0;
    assert(solana_ingress_run_local(
        TEST_PORT, on_event, &received_event, NULL, NULL
    ) == -1);
    assert(errno == EINVAL);

    pthread_t thread;
    assert(pthread_create(&thread, NULL, ingress_thread, NULL) == 0);
    struct timespec startup_delay = {
        .tv_sec = 0,
        .tv_nsec = 100000000L,
    };
    assert(nanosleep(&startup_delay, NULL) == 0);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    assert(sockfd >= 0);

    struct sockaddr_in destination = {0};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(TEST_PORT);
    assert(inet_pton(
        AF_INET, "127.0.0.1", &destination.sin_addr
    ) == 1);

    ssize_t sent = sendto(
        sockfd, packet, sizeof(packet), 0,
        (const struct sockaddr *)&destination, sizeof(destination)
    );
    assert(sent == (ssize_t)sizeof(packet));

    for (int i = 0; i < 100; ++i) {
        if (atomic_load_explicit(&received, memory_order_acquire)) {
            break;
        }
        struct timespec poll_delay = {
            .tv_sec = 0,
            .tv_nsec = 10000000L,
        };
        assert(nanosleep(&poll_delay, NULL) == 0);
    }

    assert(atomic_load_explicit(&received, memory_order_acquire));
    assert(received_event.price_raw == 15000000000ULL);
    assert(received_event.quantity_raw == 100000000ULL);
    assert(received_event.ingress_sequence_id == 1);
    assert(received_event.side == SOLANA_TRADE_SIDE_BUY);

    atomic_store_explicit(&running, false, memory_order_release);

    assert(pthread_join(thread, NULL) == 0);
    assert(ingress_result == 0);

    close(sockfd);
    printf("Solana ingress prototype test passed.\n");
    return 0;
}
