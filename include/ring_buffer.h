#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include "types.h"

// Bounded to a power-of-2 size to allow single-cycle bitwise masking
#define RING_BUFFER_SIZE 65536 // 2^16
#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1)

typedef struct {
    event_t buffer[RING_BUFFER_SIZE];
    _Atomic uint64_t head; // Read index
    _Atomic uint64_t tail; // Write index
} ring_buffer_t;

static inline void ring_buffer_init(ring_buffer_t* rb) {
    atomic_init(&rb->head, 0);
    atomic_init(&rb->tail, 0);
}

// Non-blocking single-producer enqueue loop
static inline bool ring_buffer_enqueue(ring_buffer_t* rb, const event_t* event) {
    uint64_t current_tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    uint64_t current_head = atomic_load_explicit(&rb->head, memory_order_acquire);
    
    if ((current_tail - current_head) >= RING_BUFFER_SIZE) {
        return false; // Buffer overflow (Safety drop to avoid overwrite)
    }
    
    rb->buffer[current_tail & RING_BUFFER_MASK] = *event;
    atomic_store_explicit(&rb->tail, current_tail + 1, memory_order_release);
    return true;
}

// Non-blocking single-consumer dequeue loop
static inline bool ring_buffer_dequeue(ring_buffer_t* rb, event_t* event_out) {
    uint64_t current_head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    uint64_t current_tail = atomic_load_explicit(&rb->tail, memory_order_acquire);
    
    if (current_head == current_tail) {
        return false; // Queue empty (Zero events to process)
    }
    
    *event_out = rb->buffer[current_head & RING_BUFFER_MASK];
    atomic_store_explicit(&rb->head, current_head + 1, memory_order_release);
    return true;
}

#endif // RING_BUFFER_H
