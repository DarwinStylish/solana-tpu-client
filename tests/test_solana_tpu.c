// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <assert.h>
#include "adapter_solana.h"

int main() {
    printf("[*] Running Solana TPU Client Verification...\n");
    
    /* Happy path: buy-side trade */
    solana_wire_trade_t mock_packet = {0};
    mock_packet.price_raw = 15000000000ULL; /* $150.00 */
    mock_packet.side = 0; /* Buy */
    
    event_t out;
    assert(parse_solana_borsh((const uint8_t*)&mock_packet, sizeof(mock_packet), 1, &out));
    assert(out.venue_id == VENUE_SOLANA);
    assert(out.side == 'B');
    assert(out.instrument_id == SOLANA_DEFAULT_INSTRUMENT_ID);
    
    printf("[+] Successfully deserialized Solana Borsh packet.\n");
    printf("[+] Price mapped: %ld\n", (long)out.price);

    /* Sell-side mapping */
    solana_wire_trade_t sell = {0};
    sell.side = 1;
    assert(parse_solana_borsh((const uint8_t*)&sell, sizeof(sell), 2, &out));
    assert(out.side == 'S');
    printf("[+] Sell side correctly mapped.\n");

    /* Side mapping: any non-zero side maps to 'S' */
    solana_wire_trade_t alt_side = {0};
    alt_side.side = 2;
    assert(parse_solana_borsh((const uint8_t*)&alt_side, sizeof(alt_side), 3, &out));
    assert(out.side == 'S');
    printf("[+] Non-zero side correctly mapped to Sell.\n");

    /* Negative test: buffer too short */
    assert(parse_solana_borsh((const uint8_t*)&mock_packet, sizeof(mock_packet) - 1, 4, &out) == false);
    printf("[+] Buffer too short correctly rejected.\n");
    
    printf("\n[SUCCESS] Solana TPU tests passed.\n");
    return 0;
}
