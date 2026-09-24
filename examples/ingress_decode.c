// SPDX-License-Identifier: Apache-2.0

#include "solana/ingress.h"

#include <stdint.h>

int main(void) {
    uint8_t wire[SOLANA_INGRESS_WIRE_SIZE] = {0};
    solana_ingress_event_t event = {0};

    wire[32] = SOLANA_TRADE_SIDE_BUY;

    if (!solana_ingress_decode(
            wire,
            sizeof(wire),
            UINT64_C(1),
            &event)) {
        return 1;
    }

    if (event.ingress_sequence_id != UINT64_C(1)) {
        return 1;
    }

    if (event.side != SOLANA_TRADE_SIDE_BUY) {
        return 1;
    }

    return 0;
}
