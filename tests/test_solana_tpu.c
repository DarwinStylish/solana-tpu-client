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
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "adapter_solana.h"
#include "ring_buffer.h"

volatile bool running = true;
ring_buffer_t ingress_queue;
#define TEST_PORT 49000

void* adapter_thread_func(void* arg) {
    (void)arg;
    solana_adapter_run(TEST_PORT, &ingress_queue, &running);
    return NULL;
}

int main() {
    printf("[*] Running Solana TPU Client Verification...\n");
    
    ring_buffer_init(&ingress_queue);

    // 1. Start the adapter on a separate thread
    pthread_t adapter_thread;
    assert(pthread_create(&adapter_thread, NULL, adapter_thread_func, NULL) == 0);

    // Wait briefly for the adapter to bind the UDP port
    usleep(100000);

    // 2. Prepare a mock wire packet
    solana_wire_trade_t mock_packet = {0};
    mock_packet.price_raw = 15000000000ULL; /* $150.00 */
    mock_packet.side = 0; /* Buy */
    
    // 3. Send it to the adapter over localhost UDP
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    assert(sock >= 0);

    struct sockaddr_in dest = {0};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(TEST_PORT);
    inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr);

    ssize_t sent = sendto(sock, &mock_packet, sizeof(mock_packet), 0, (struct sockaddr*)&dest, sizeof(dest));
    assert(sent == sizeof(mock_packet));

    // 4. Poll the ring buffer for the parsed event
    event_t out;
    bool received = false;
    for (int i = 0; i < 100; i++) {
        if (ring_buffer_dequeue(&ingress_queue, &out)) {
            received = true;
            break;
        }
        usleep(10000); // 10ms
    }

    assert(received == true);
    assert(out.venue_id == VENUE_SOLANA);
    assert(out.side == 'B');
    assert(out.instrument_id == SOLANA_DEFAULT_INSTRUMENT_ID);
    assert(out.price == (fixed_t)15000000000ULL);
    
    printf("[+] Successfully received and deserialized Solana UDP datagram.\n");

    // 5. Shutdown the adapter
    running = false;
    
    // Send a dummy packet to wake up recvfrom if it was blocking
    // (though in our implementation it's non-blocking busy-poll, this doesn't hurt)
    sendto(sock, &mock_packet, sizeof(mock_packet), 0, (struct sockaddr*)&dest, sizeof(dest));
    
    pthread_join(adapter_thread, NULL);
    close(sock);

    printf("\n[SUCCESS] Solana TPU network integration tests passed.\n");
    return 0;
}
