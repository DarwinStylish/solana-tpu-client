// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/ingress.h"

#include <arpa/inet.h>
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define TEST_PORT 49000

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

static void *ingress_thread(void *arg) {
    (void)arg;
    ingress_result = solana_ingress_run_local(
        TEST_PORT, on_event, &received_event, &running
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
    assert(!solana_ingress_decode(packet, 32, 1, &decoded));

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
