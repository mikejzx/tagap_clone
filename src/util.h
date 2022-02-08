#ifndef UTIL_H
#define UTIL_H

// Get max of two scalars
// Note that second type is casted to the first
#ifndef max
#define max(a, b) ({\
    __typeof__ (a) _a = (a); \
    __typeof__ (a) _b = (b); \
    _a > _b ? _a : _b; })
#endif // max(a, b)

// Get min of two scalars
// Note that second type is casted to the first
#ifndef min
#define min(a, b) ({\
    __typeof__ (a) _a = (a); \
    __typeof__ (a) _b = (b); \
    _a < _b ? _a : _b; })
#endif // min(a, b)

// Clamp scalar between a maximum and minimum.
#ifndef clamp
#define clamp(x, mn, mx) ({\
    __typeof__ (x) _x = (x); \
    __typeof__ (mn) _mn = (mn); \
    __typeof__ (mx) _mx = (mx); \
    max(_mn, min(_mx, _x)); })
#endif // clamp(x, mn, mx)

#define clamp01(x) max(0.0f, min((x), 1.0f))

#define within(low, x, high) \
    ({ (x >= low) && (x <= high); })

#define lerpf(_a, _b, _t) \
    ({ __typeof__(_t) _u = clamp01(_t); ((_a) * (1 - _u)) + ((_b) * _u); })

#define sinerpf(_a, _b, _t) \
    lerpf((_a), (_b), sinf((_t) * 3.1415f * 0.5f))

#endif