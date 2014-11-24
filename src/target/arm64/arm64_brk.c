#include <sys/syscall.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>

#include "arm64_syscall.h"

/* code stolen from noir. Not re-entrant but should be ok since higher lock level is certainly taken */
#define PAGE_SIZE                       4096
#define PAGE_MASK                       (PAGE_SIZE - 1)
/* reduce min brk size to 16K bytes to align with ltp (kernel ?) */
#define MIN_BRK_SIZE                    (4 * PAGE_SIZE)

extern guest_ptr startbrk_64;
static guest_ptr startbrk_64_aligned;
static uint64_t curbrk;
static uint64_t mapbrk;

/* Reserve space so brk as a minimun guarantee space */
void arm64_setup_brk()
{
    //init phase
    if (!curbrk) {
        guest_ptr map_result;

        startbrk_64_aligned = ((startbrk_64 + PAGE_SIZE - 1) & ~PAGE_MASK);
        curbrk = startbrk_64_aligned;
        mapbrk = startbrk_64_aligned;
        map_result = mmap_guest(mapbrk, MIN_BRK_SIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
        if (map_result == mapbrk) {
            mapbrk = mapbrk + MIN_BRK_SIZE;
        } else
            assert(0);
    }
}

long arm64_brk(struct arm64_target *context)
{
    guest_ptr new_brk = context->regs.r[0];

    //sanity check
    if (new_brk < startbrk_64_aligned)
        goto out;
    if (new_brk) {
        // need to map new memory ?
        if (new_brk >= mapbrk) {
            guest_ptr new_mapbrk = ((new_brk + PAGE_SIZE) & ~PAGE_MASK);
            guest_ptr map_result;

            map_result = mmap_guest(mapbrk, new_mapbrk - mapbrk, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
            if (map_result != mapbrk)
                goto out;
            mapbrk = new_mapbrk;
        }
        // init mem with zero
        if (new_brk > curbrk)
            memset(g_2_h(curbrk), 0, new_brk - curbrk);
    }

    curbrk = new_brk;
out:
    return curbrk;
}
