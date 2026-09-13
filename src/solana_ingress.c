// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Okot Darwin Clay

#include "solana/ingress.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SOLANA_MAX_DATAGRAM_SIZE 1500U
#define OFFSET_INSTRUCTION_TYPE 0U
#define OFFSET_TIMESTAMP_MS 8U
#define OFFSET_PRICE_RAW 16U
#define OFFSET_QUANTITY_RAW 24U
#define OFFSET_SIDE 32U

static uint64_t load_u64_le(const uint8_t *p) {
    return ((uint64_t)p[0]) |
           ((uint64_t)p[1] << 8) |
           ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) |
           ((uint64_t)p[4] << 32) |
           ((uint64_t)p[5] << 40) |
           ((uint64_t)p[6] << 48) |
           ((uint64_t)p[7] << 56);
}

static int close_preserving_errno(int fd, int saved_errno) {
    (void)close(fd);
    errno = saved_errno;
    return -1;
}

bool solana_ingress_decode(
    const uint8_t *wire_buffer,
    size_t wire_len,
    uint64_t ingress_sequence_id,
    solana_ingress_event_t *out_event
) {
    if (wire_buffer == NULL || out_event == NULL ||
        wire_len != SOLANA_INGRESS_WIRE_SIZE) {
        return false;
    }

    uint8_t side = wire_buffer[OFFSET_SIDE];
    if (side > SOLANA_TRADE_SIDE_SELL) {
        return false;
    }

    memset(out_event, 0, sizeof(*out_event));
    out_event->instruction_type =
        load_u64_le(wire_buffer + OFFSET_INSTRUCTION_TYPE);
    out_event->source_timestamp_ms =
        load_u64_le(wire_buffer + OFFSET_TIMESTAMP_MS);
    out_event->price_raw =
        load_u64_le(wire_buffer + OFFSET_PRICE_RAW);
    out_event->quantity_raw =
        load_u64_le(wire_buffer + OFFSET_QUANTITY_RAW);
    out_event->ingress_sequence_id = ingress_sequence_id;
    out_event->side = side;

    return true;
}

int solana_ingress_run_local(
    uint16_t port,
    solana_ingress_event_fn on_event,
    void *event_context,
    solana_ingress_continue_fn should_continue,
    void *control_context
) {
    if (on_event == NULL || should_continue == NULL) {
        errno = EINVAL;
        return -1;
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        int saved_errno = errno;
        return close_preserving_errno(sockfd, saved_errno);
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        int saved_errno = errno;
        return close_preserving_errno(sockfd, saved_errno);
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        int saved_errno = errno;
        return close_preserving_errno(sockfd, saved_errno);
    }

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr *)&address,
             sizeof(address)) < 0) {
        int saved_errno = errno;
        return close_preserving_errno(sockfd, saved_errno);
    }

    uint8_t buffer[SOLANA_MAX_DATAGRAM_SIZE];
    uint64_t sequence = 0;

    while (should_continue(control_context)) {
        ssize_t received = recvfrom(
            sockfd, buffer, sizeof(buffer), 0, NULL, NULL
        );

        if (received > 0) {
            solana_ingress_event_t event;
            if (solana_ingress_decode(
                    buffer, (size_t)received, ++sequence, &event)) {
                on_event(&event, event_context);
            }
            continue;
        }

        if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            int saved_errno = errno;
            return close_preserving_errno(sockfd, saved_errno);
        }
    }

    return close(sockfd);
}
