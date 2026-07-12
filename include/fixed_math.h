#ifndef FIXED_MATH_H
#define FIXED_MATH_H

#include <stdint.h>

#define SCALE 100000000LL        // 10^8
#define HALF_SCALE 50000000LL   // 10^8 / 2

typedef int64_t fixed_t;

// High-performance fixed-point multiplication with symmetric rounding
static inline fixed_t fixed_mul(fixed_t a, fixed_t b) {
    __int128 intermediate = (__int128)a * b;
    if (intermediate < 0) {
        intermediate -= HALF_SCALE;
    } else {
        intermediate += HALF_SCALE;
    }
    return (fixed_t)(intermediate / SCALE);
}

// High-performance fixed-point division with symmetric rounding
static inline fixed_t fixed_div(fixed_t a, fixed_t b) {
    if (__builtin_expect((b == 0), 0)) {
        return 0; // Guard against division by zero crashes
    }
    __int128 numerator = (__int128)a * SCALE;
    __int128 half_b = b / 2;
    if (numerator < 0) {
        numerator -= half_b;
    } else {
        numerator += half_b;
    }
    return (fixed_t)(numerator / b);
}

// Helper converters
static inline fixed_t from_double(double d) {
    return (fixed_t)(d * SCALE + (d < 0 ? -0.5 : 0.5));
}

static inline double to_double(fixed_t f) {
    return (double)f / SCALE;
}

#endif // FIXED_MATH_H
