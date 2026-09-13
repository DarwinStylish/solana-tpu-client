// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/ingress.h"

#include <cstdint>

int main() {
    std::uint8_t packet[SOLANA_INGRESS_WIRE_SIZE] = {};
    solana_ingress_event_t event = {};
    (void)solana_ingress_decode(
        packet, sizeof(packet), 0, &event
    );
    return 0;
}
