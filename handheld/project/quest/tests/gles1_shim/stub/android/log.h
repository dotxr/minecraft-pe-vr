// Host stand-in for the Android logging header (test harness only).
#ifndef HOST_ANDROID_LOG_H
#define HOST_ANDROID_LOG_H
#include <cstdio>
#include <cstdarg>
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_ERROR 6
inline int __android_log_print(int prio, const char* tag, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%s] ", tag);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    return 0;
}
#endif
