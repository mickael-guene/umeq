#include <sys/mman.h>

#include "target64.h"
#include "arm64_private.h"

guest_ptr mmap_guest(guest_ptr addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	return (uint64_t) mmap((void *) addr, length, prot, flags, fd, offset);
}

int munmap_guest(guest_ptr addr, size_t length)
{
	return munmap((void *) addr, length);
}
