#include <sys/syscall.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>

#include "arm_syscall.h"

/* code stolen from noir. Not re-entrant but should be ok since higher lock level is certainly taken */
#define PAGE_SIZE                       4096
#define PAGE_MASK                       (PAGE_SIZE - 1)
extern unsigned int startbrk; //from loader
int arm_brk(struct arm_target *context)
{
    static unsigned int curbrk;
    static unsigned int mapbrk;
	unsigned int new_brk = context->regs.r[0];

	//init phase
	if (!curbrk) {
		curbrk = startbrk;
		mapbrk = ((startbrk + PAGE_SIZE) & ~PAGE_MASK);
	}
	//sanity check
	if (new_brk < startbrk)
		goto out;
	if (new_brk) {
		// need to map new memory ?
		if (new_brk >= mapbrk) {
			unsigned int new_mapbrk = ((new_brk + PAGE_SIZE) & ~PAGE_MASK);
			unsigned int map_result;

			map_result = (unsigned int)(long) mmap((void *)(long)mapbrk, new_mapbrk - mapbrk, PROT_READ|PROT_WRITE,
													MAP_ANON|MAP_PRIVATE, -1, 0);
			if (map_result != mapbrk)
				goto out;
			mapbrk = new_mapbrk;
		}
		// init mem with zero
		if (new_brk > curbrk)
			memset((void *)(long)curbrk, 0, new_brk - curbrk);
	}

	curbrk = new_brk;
out:
	return curbrk;
}
