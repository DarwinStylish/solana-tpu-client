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

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <inttypes.h>

#include "adapter_solana.h"
#include "ring_buffer.h"

#define NUM_EVENTS 10000000ULL // 10 million events

static ring_buffer_t ingress_queue;

static inline uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main() {
    printf("[*] Initializing Solana Gateway Benchmark...\n");
    
    ring_buffer_init(&ingress_queue);

    // 1. Prepare a mock wire packet matching solana_wire_trade_t size and layout
    solana_wire_trade_t mock_packet = {0};
    mock_packet.instruction_type = 1;
    mock_packet.timestamp_ms = 1690000000000ULL;
    mock_packet.price_raw = 15000000000ULL; // $150.00
    mock_packet.quantity_raw = 100000000ULL; // 1 unit
    mock_packet.side = 0; // Buy

    // Treat it as a raw buffer as it comes off the UDP socket
    uint8_t* wire_buffer = (uint8_t*)&mock_packet;
    size_t wire_len = sizeof(solana_wire_trade_t);

    printf("[*] Starting zero-copy parse execution loop for %llu packets...\n", NUM_EVENTS);
    
    event_t parsed_event;
    uint64_t start_time = get_time_ns();

    // Hot loop
    for (uint64_t i = 0; i < NUM_EVENTS; i++) {
        // 1. Parse Borsh packet using zero-copy pointer arithmetic
        bool parsed = parse_solana_borsh(wire_buffer, wire_len, i, &parsed_event);
        
        // 2. Enqueue into lock-free ring buffer
        if (parsed) {
            ring_buffer_enqueue(&ingress_queue, &parsed_event);
        }
        
        // 3. Immediately dequeue to prevent filling up the queue
        event_t dummy;
        ring_buffer_dequeue(&ingress_queue, &dummy);
    }

    uint64_t end_time = get_time_ns();
    uint64_t duration_ns = end_time - start_time;
    double duration_sec = (double)duration_ns / 1e9;
    
    double ops_per_sec = (double)NUM_EVENTS / duration_sec;
    double ns_per_op = (double)duration_ns / (double)NUM_EVENTS;

    printf("\n=========================================\n");
    printf("        SOLANA PARSER BENCHMARK          \n");
    printf("=========================================\n");
    printf("Packets Parsed   : %llu\n", NUM_EVENTS);
    printf("Total Duration   : %.6f seconds\n", duration_sec);
    printf("Throughput       : %.2f ops/sec\n", ops_per_sec);
    printf("Average Latency  : %.2f ns/packet\n", ns_per_op);
    printf("=========================================\n");

    return 0;
}
