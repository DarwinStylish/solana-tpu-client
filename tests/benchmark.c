// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#define _POSIX_C_SOURCE 199309L

#include "solana/ingress.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static const uint64_t NUM_EVENTS = UINT64_C(10000000);

static uint64_t get_time_ns(void) {
    struct timespec ts;
    assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
    return (uint64_t)ts.tv_sec * 1000000000ULL +
           (uint64_t)ts.tv_nsec;
}

static void store_u64_le(uint8_t *p, uint64_t value) {
    for (unsigned int i = 0; i < 8; ++i) {
        p[i] = (uint8_t)(value >> (i * 8));
    }
}

int main(void) {
    uint8_t packet[SOLANA_INGRESS_WIRE_SIZE] = {0};
    store_u64_le(packet + 0, 1);
    store_u64_le(packet + 8, 1690000000000ULL);
    store_u64_le(packet + 16, 15000000000ULL);
    store_u64_le(packet + 24, 100000000ULL);
    packet[32] = SOLANA_TRADE_SIDE_BUY;

    solana_ingress_event_t event;
    uint64_t checksum = 0;
    uint64_t start = get_time_ns();

    for (uint64_t i = 0; i < NUM_EVENTS; ++i) {
        bool parsed = solana_ingress_decode(
            packet, sizeof(packet), i, &event
        );
        assert(parsed);
        checksum ^= event.price_raw;
        checksum ^= event.quantity_raw;
        checksum ^= event.ingress_sequence_id;
    }

    uint64_t duration = get_time_ns() - start;
    double seconds = (double)duration / 1e9;

    printf("Events decoded: %" PRIu64 "\n", NUM_EVENTS);
    printf("Duration: %.6f seconds\n", seconds);
    printf("Throughput: %.2f events/second\n",
           (double)NUM_EVENTS / seconds);
    printf("Average decode latency: %.2f ns/event\n",
           (double)duration / (double)NUM_EVENTS);
    printf("Checksum: %" PRIu64 "\n", checksum);

    return 0;
}
