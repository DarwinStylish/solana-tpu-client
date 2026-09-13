// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/ingress.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void test_lengths(void) {
    uint8_t buffer[65] = {0};
    buffer[32] = SOLANA_TRADE_SIDE_BUY;

    for (size_t len = 0; len <= sizeof(buffer); ++len) {
        solana_ingress_event_t event;
        bool decoded = solana_ingress_decode(
            buffer, len, UINT64_C(7), &event
        );

        if (len == SOLANA_INGRESS_WIRE_SIZE) {
            assert(decoded);
        } else {
            assert(!decoded);
        }
    }

    const size_t rejected_lengths[] = {
        66U,
        255U,
        4096U,
        SIZE_MAX,
    };

    for (size_t i = 0;
         i < sizeof(rejected_lengths) / sizeof(rejected_lengths[0]);
         ++i) {
        solana_ingress_event_t event;
        assert(!solana_ingress_decode(
            buffer,
            rejected_lengths[i],
            UINT64_C(7),
            &event
        ));
    }
}

static void test_side_domain(void) {
    uint8_t buffer[SOLANA_INGRESS_WIRE_SIZE] = {0};

    for (unsigned int side = 0; side <= UINT8_MAX; ++side) {
        solana_ingress_event_t event;
        buffer[32] = (uint8_t)side;

        bool decoded = solana_ingress_decode(
            buffer, sizeof(buffer), UINT64_C(11), &event
        );

        if (side == SOLANA_TRADE_SIDE_BUY ||
            side == SOLANA_TRADE_SIDE_SELL) {
            assert(decoded);
            assert(event.side == (uint8_t)side);
        } else {
            assert(!decoded);
        }
    }
}

static void test_little_endian_fields(void) {
    uint8_t buffer[SOLANA_INGRESS_WIRE_SIZE];
    for (size_t i = 0; i < sizeof(buffer); ++i) {
        buffer[i] = (uint8_t)i;
    }
    buffer[32] = SOLANA_TRADE_SIDE_SELL;

    solana_ingress_event_t event;
    memset(&event, 0xA5, sizeof(event));

    assert(solana_ingress_decode(
        buffer,
        sizeof(buffer),
        UINT64_C(0xFEDCBA9876543210),
        &event
    ));

    assert(event.instruction_type ==
           UINT64_C(0x0706050403020100));
    assert(event.source_timestamp_ms ==
           UINT64_C(0x0F0E0D0C0B0A0908));
    assert(event.price_raw ==
           UINT64_C(0x1716151413121110));
    assert(event.quantity_raw ==
           UINT64_C(0x1F1E1D1C1B1A1918));
    assert(event.ingress_sequence_id ==
           UINT64_C(0xFEDCBA9876543210));
    assert(event.side == SOLANA_TRADE_SIDE_SELL);

    for (size_t i = 0; i < sizeof(event.reserved); ++i) {
        assert(event.reserved[i] == 0);
    }
}

static void test_sequence_extremes(void) {
    uint8_t buffer[SOLANA_INGRESS_WIRE_SIZE] = {0};
    buffer[32] = SOLANA_TRADE_SIDE_BUY;

    const uint64_t sequences[] = {
        UINT64_C(0),
        UINT64_C(1),
        UINT64_MAX,
    };

    for (size_t i = 0;
         i < sizeof(sequences) / sizeof(sequences[0]);
         ++i) {
        solana_ingress_event_t event;
        assert(solana_ingress_decode(
            buffer, sizeof(buffer), sequences[i], &event
        ));
        assert(event.ingress_sequence_id == sequences[i]);
    }
}

int main(void) {
    test_lengths();
    test_side_domain();
    test_little_endian_fields();
    test_sequence_extremes();
    return 0;
}
