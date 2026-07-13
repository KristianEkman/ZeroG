#ifndef WIN_PTHREAD_H
#define WIN_PTHREAD_H

#ifdef _WIN32
#include <windows.h>

typedef HANDLE pthread_t;

static inline int pthread_create(pthread_t *thread, const void *attr, void *(*start_routine)(void*), void *arg) {
    (void)attr;
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)start_routine, arg, 0, NULL);
    return (*thread == NULL) ? -1 : 0;
}

static inline int pthread_join(pthread_t thread, void **retval) {
    (void)retval;
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}
#endif

#endif /* WIN_PTHREAD_H */
