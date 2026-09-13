// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#ifndef SOLANA_INGRESS_H
#define SOLANA_INGRESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SOLANA_INGRESS_WIRE_SIZE 33U
#define SOLANA_INGRESS_EVENT_SIZE 48U

typedef uint8_t solana_trade_side_t;

#define SOLANA_TRADE_SIDE_BUY UINT8_C(0)
#define SOLANA_TRADE_SIDE_SELL UINT8_C(1)

typedef struct {
    uint64_t instruction_type;
    uint64_t source_timestamp_ms;
    uint64_t price_raw;
    uint64_t quantity_raw;
    uint64_t ingress_sequence_id;
    solana_trade_side_t side;
    uint8_t reserved[7];
} solana_ingress_event_t;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The event pointer passed to this callback is valid only for the
 * duration of the callback. Callers that retain an event must copy it.
 * The callback executes synchronously on the ingress-running thread.
 */
typedef void (*solana_ingress_event_fn)(
    const solana_ingress_event_t *event,
    void *context
);

/* Return nonzero to continue polling and zero to stop. */
typedef int (*solana_ingress_continue_fn)(void *context);

/*
 * Decode exactly one fixed-layout prototype event.
 * Returns true only when wire_len is exactly SOLANA_INGRESS_WIRE_SIZE
 * and all currently validated fields are accepted.
 */
bool solana_ingress_decode(
    const uint8_t *wire_buffer,
    size_t wire_len,
    uint64_t ingress_sequence_id,
    solana_ingress_event_t *out_event
);

/*
 * Run the loopback-only prototype receiver.
 * event_context and control_context are caller-owned and may be NULL.
 * Returns 0 after should_continue requests shutdown.
 * Returns -1 on setup, receive, or close failure; errno identifies
 * the failure. NULL callback functions fail with errno == EINVAL.
 */
int solana_ingress_run_local(
    uint16_t port,
    solana_ingress_event_fn on_event,
    void *event_context,
    solana_ingress_continue_fn should_continue,
    void *control_context
);

#ifdef __cplusplus
}
#endif

#endif
