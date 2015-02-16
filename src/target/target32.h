#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __TARGET32__
#define __TARGET32__ 1

typedef uint32_t guest_ptr;
#define g_2_h(ptr)  ((void *)(uint64_t)((ptr) + (mmap_offset)))
#define h_2_g(ptr)  ((guest_ptr) (((uint64_t)(ptr)) - (mmap_offset)))

extern uint64_t mmap_offset;
extern guest_ptr mmap_guest(guest_ptr addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int munmap_guest(guest_ptr addr, size_t length);
extern void *munmap_guest_ongoing(guest_ptr addr, size_t length);

#endif

#ifdef __cplusplus
}
#endif
