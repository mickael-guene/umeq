#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __TARGET32__
#define __TARGET32__ 1


typedef uint32_t guest_ptr;
#if 1
	#define g_2_h(ptr)  ((void *)(uint64_t)((ptr) + (mmap_offset)))
	#define h_2_g(ptr)  ((guest_ptr) (((uint64_t)(ptr)) - (mmap_offset)))
#else
/* these ones don't translate null pointer for testing purpose */
	#define g_2_h(ptr)	((ptr)?((void *)(uint64_t)((ptr) + (mmap_offset))):NULL)
	#define h_2_g(ptr)	((ptr)?((guest_ptr) (((uint64_t)(ptr)) - (mmap_offset))):0)
#endif

extern uint64_t mmap_offset;
extern guest_ptr mmap_guest(guest_ptr addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int munmap_guest(guest_ptr addr, size_t length);

#endif

#ifdef __cplusplus
}
#endif
