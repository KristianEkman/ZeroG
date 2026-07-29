#ifndef WIN_UNISTD_H
#define WIN_UNISTD_H

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <io.h>

#define _SC_NPROCESSORS_ONLN 1

static inline long sysconf(int name) {
    if (name == _SC_NPROCESSORS_ONLN) {
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return (long)sysinfo.dwNumberOfProcessors;
    }
    return -1;
}

#endif

#endif /* WIN_UNISTD_H */
