// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/ingress.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static uint64_t load_u64_le_oracle(const uint8_t *p) {
    return ((uint64_t)p[0]) |
           ((uint64_t)p[1] << 8) |
           ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) |
           ((uint64_t)p[4] << 32) |
           ((uint64_t)p[5] << 40) |
           ((uint64_t)p[6] << 48) |
           ((uint64_t)p[7] << 56);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    const uint64_t sequence = UINT64_C(0xA5A55A5AF0F00F0F);
    solana_ingress_event_t event;

    bool decoded = solana_ingress_decode(
        data, size, sequence, &event
    );

    if (size != SOLANA_INGRESS_WIRE_SIZE) {
        assert(!decoded);
        return 0;
    }

    if (data[32] != SOLANA_TRADE_SIDE_BUY &&
        data[32] != SOLANA_TRADE_SIDE_SELL) {
        assert(!decoded);
        return 0;
    }

    assert(decoded);
    assert(event.instruction_type ==
           load_u64_le_oracle(data + 0));
    assert(event.source_timestamp_ms ==
           load_u64_le_oracle(data + 8));
    assert(event.price_raw ==
           load_u64_le_oracle(data + 16));
    assert(event.quantity_raw ==
           load_u64_le_oracle(data + 24));
    assert(event.ingress_sequence_id == sequence);
    assert(event.side == data[32]);

    for (size_t i = 0; i < sizeof(event.reserved); ++i) {
        assert(event.reserved[i] == 0);
    }

    return 0;
}
