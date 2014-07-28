#ifdef __cplusplus
extern "C" {
#endif

#ifndef __RUNTIME__
#define __RUNTIME__ 1

extern void debug(const char *format, ...);
extern void info(const char *format, ...);
extern void warning(const char *format, ...);
extern void error(const char *format, ...);
extern void fatal(const char *format, ...);
extern void *mmap_guest(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

#endif

#ifdef __cplusplus
}
#endif
