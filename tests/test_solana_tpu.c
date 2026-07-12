#include <stdio.h>
#include <assert.h>
#include "adapter_solana.h"

int main() {
    printf("[*] Running Solana TPU Client Verification...\n");
    
    // Simulate raw packet from the wire
    uint8_t raw_packet[33] = {0};
    solana_wire_trade_t* mock_packet = (solana_wire_trade_t*)raw_packet;
    mock_packet->price_raw = 15000000000ULL; // $150.00
    
    event_t out;
    assert(parse_solana_borsh(raw_packet, &out));
    
    printf("[+] Successfully deserialized Solana Borsh packet.\n");
    printf("[+] Price mapped: %ld\n", out.price);
    
    return 0;
}
