#include <stdio.h>
#include <assert.h>
#include "adapter_solana.h"

int main() {
    printf("[*] Running Solana TPU Client Verification...\n");
    
    /* Happy path: buy-side trade */
    uint8_t raw_packet[33] = {0};
    solana_wire_trade_t* mock_packet = (solana_wire_trade_t*)raw_packet;
    mock_packet->price_raw = 15000000000ULL; /* $150.00 */
    mock_packet->side = 0; /* Buy */
    
    event_t out;
    assert(parse_solana_borsh(raw_packet, &out));
    assert(out.venue_id == VENUE_SOLANA);
    assert(out.side == 'B');
    assert(out.instrument_id == SOLANA_DEFAULT_INSTRUMENT_ID);
    
    printf("[+] Successfully deserialized Solana Borsh packet.\n");
    printf("[+] Price mapped: %ld\n", (long)out.price);

    /* Sell-side mapping */
    uint8_t sell_pkt[33] = {0};
    solana_wire_trade_t* sell = (solana_wire_trade_t*)sell_pkt;
    sell->side = 1;
    assert(parse_solana_borsh(sell_pkt, &out));
    assert(out.side == 'S');
    printf("[+] Sell side correctly mapped.\n");
    
    printf("\n[SUCCESS] Solana TPU tests passed.\n");
    return 0;
}
