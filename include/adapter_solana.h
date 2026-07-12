#ifndef ADAPTER_SOLANA_H
#define ADAPTER_SOLANA_H

#include "types.h"
#include <string.h>
#include <stdbool.h>

#pragma pack(push, 1)
typedef struct {
    uint64_t instruction_type; 
    uint64_t timestamp_ms;     
    uint64_t price_raw;        
    uint64_t quantity_raw;     
    uint8_t  side;             
} solana_wire_trade_t;
#pragma pack(pop)

static inline bool parse_solana_borsh(const uint8_t* wire_buffer, event_t* out_event) {
    const solana_wire_trade_t* wire_data = (const solana_wire_trade_t*)wire_buffer;
    
    out_event->receive_timestamp_ns = wire_data->timestamp_ms * 1000000ULL;
    out_event->price = (fixed_t)wire_data->price_raw; 
    out_event->quantity = (fixed_t)wire_data->quantity_raw;
    out_event->side = (wire_data->side == 0) ? 'B' : 'S';
    out_event->venue_id = VENUE_SOLANA;
    out_event->event_type = EVENT_MARKET_TICK;
    out_event->instrument_id = 1; 
    
    return true;
}

#endif // ADAPTER_SOLANA_H
