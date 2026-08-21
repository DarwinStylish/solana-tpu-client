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
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#include "adapter_solana.h"

#define SOLANA_MAX_DATAGRAM_SIZE 1500

/**
 * @brief Main network polling loop for the Solana TPU adapter.
 *
 * This function should be spawned on an isolated CPU core. It busy-polls
 * a UDP socket for incoming Borsh datagrams from the Solana cluster,
 * parses them zero-allocation, and enqueues them to the engine.
 *
 * @param port UDP port to listen on.
 * @param ingress_queue SPSC ring buffer for engine ingress.
 * @param running Volatile shutdown flag.
 */
void solana_adapter_run(uint16_t port, ring_buffer_t* ingress_queue, volatile bool* running) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("[-] Failed to create Solana UDP socket");
        return;
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("[-] Failed to set SO_REUSEADDR | SO_REUSEPORT");
        close(sockfd);
        return;
    }

    /* Set non-blocking for busy-polling */
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("[-] Failed to set O_NONBLOCK");
        close(sockfd);
        return;
    }

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("[-] Solana adapter bind failed");
        close(sockfd);
        return;
    }

    printf("[*] Solana TPU Adapter listening on UDP port %u\n", port);

    uint8_t buffer[SOLANA_MAX_DATAGRAM_SIZE];
    uint64_t seq_id = 0;

    while (*running) {
        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
        if (n > 0) {
            event_t event;
            if (parse_solana_borsh(buffer, (size_t)n, ++seq_id, &event)) {
                /* Push to the lock-free ring buffer (drop on full) */
                if (!ring_buffer_enqueue(ingress_queue, &event)) {
                    /* Packet dropped due to full buffer - safety path */
                }
            }
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("[-] Solana recvfrom error");
            break;
        }
    }

    close(sockfd);
    printf("[*] Solana TPU Adapter shutdown complete.\n");
}
