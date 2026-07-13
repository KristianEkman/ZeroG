#ifndef MSVC_COMPAT_H
#define MSVC_COMPAT_H

#ifdef _MSC_VER
#include <intrin.h>

// Map popcount
#define __builtin_popcountll __popcnt64

// Map ctzll (count trailing zeros) using MSVC intrinsic _BitScanForward64
static inline int msvc_ctzll(unsigned __int64 mask) {
    unsigned long index;
    if (_BitScanForward64(&index, mask)) {
        return (int)index;
    }
    return 0;
}
#define __builtin_ctzll msvc_ctzll

#endif

#endif /* MSVC_COMPAT_H */
