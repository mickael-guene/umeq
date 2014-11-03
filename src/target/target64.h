#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __TARGET64__
#define __TARGET64__ 1

typedef uint64_t guest_ptr;
#define g_2_h(ptr)  ((void *)(ptr))
#define h_2_g(ptr)  ((uint64_t)(ptr))

extern guest_ptr mmap_guest(guest_ptr addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int munmap_guest(guest_ptr addr, size_t length);

#endif

#ifdef __cplusplus
}
#endif
