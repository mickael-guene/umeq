#ifdef __cplusplus
extern "C" {
#endif

#ifndef __RUNTIME__
#define __RUNTIME__ 1

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

extern void debug(const char *format, ...);
extern void info(const char *format, ...);
extern void warning(const char *format, ...);
extern void error(const char *format, ...);
extern void fatal_internal(const char *format, ...);
#define fatal(...) \
do { \
fatal_internal(__VA_ARGS__); \
assert(0); \
} while(0)

#endif

#ifdef __cplusplus
}
#endif
