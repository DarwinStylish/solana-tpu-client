#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include "fixed_math.h"

typedef enum {
    EVENT_MARKET_TICK = 1,
    EVENT_ORDER_ACK   = 2,
    EVENT_ORDER_FILL  = 3,
    EVENT_FUNDING     = 4
} event_type_t;

typedef enum {
    VENUE_MONAD        = 1,
    VENUE_HYPERLIQUID  = 2,
    VENUE_SOLANA       = 3
} venue_id_t;

// Meticulously forced to match a physical 64-byte hardware cache line
__attribute__((aligned(64)))
typedef struct {
    uint64_t     receive_timestamp_ns; // Ingress timestamp (PTP boundary)
    uint64_t     exchange_sequence_id; // Strict monotonic sequencer ID
    fixed_t      price;                // Scaled 10^8
    fixed_t      quantity;             // Scaled 10^8
    uint32_t     instrument_id;        // Unified token mapping ID
    uint8_t      event_type;           // event_type_t
    uint8_t      venue_id;             // venue_id_t
    uint8_t      side;                 // 'B' for Buy, 'S' for Sell
    uint8_t      _padding[21];         // Explicit pad guarantees 64-byte alignment boundaries
} event_t;

#endif // TYPES_H
