#ifndef WIN_SYS_TIME_H
#define WIN_SYS_TIME_H

#ifdef _WIN32
#include <windows.h>
#include <winsock.h>

static inline int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    if (tv) {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        unsigned __int64 tmp = 0;
        tmp |= ft.dwHighDateTime;
        tmp <<= 32;
        tmp |= ft.dwLowDateTime;
        
        // Convert to microseconds since Jan 1, 1970
        tmp /= 10; // Convert 100-nanosecond intervals to microseconds
        tmp -= 11644473600000000ULL; // Subtract epoch difference
        tv->tv_sec = (long)(tmp / 1000000UL);
        tv->tv_usec = (long)(tmp % 1000000UL);
    }
    return 0;
}
#endif

#endif /* WIN_SYS_TIME_H */
