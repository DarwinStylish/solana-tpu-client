// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#ifndef SOLANA_INGRESS_H
#define SOLANA_INGRESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#define SOLANA_INGRESS_WIRE_SIZE 33U

typedef enum {
    SOLANA_TRADE_SIDE_BUY = 0,
    SOLANA_TRADE_SIDE_SELL = 1
} solana_trade_side_t;

typedef struct {
    uint64_t instruction_type;
    uint64_t source_timestamp_ms;
    uint64_t price_raw;
    uint64_t quantity_raw;
    uint64_t ingress_sequence_id;
    solana_trade_side_t side;
} solana_ingress_event_t;

typedef void (*solana_ingress_event_fn)(
    const solana_ingress_event_t *event,
    void *context
);

bool solana_ingress_decode(
    const uint8_t *wire_buffer,
    size_t wire_len,
    uint64_t ingress_sequence_id,
    solana_ingress_event_t *out_event
);

int solana_ingress_run_local(
    uint16_t port,
    solana_ingress_event_fn on_event,
    void *context,
    atomic_bool *running
);

#endif
