#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __TARGET64__
#define __TARGET64__ 1

typedef uint64_t guest64_ptr;

#define g_2_h_64(ptr)  ((void *)(ptr))
#define h_2_g_64(ptr)  ((uint64_t)(ptr))

extern guest64_ptr mmap64_guest(guest64_ptr addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int munmap64_guest(guest64_ptr addr, size_t length);

#endif

#ifdef __cplusplus
}
#endif
