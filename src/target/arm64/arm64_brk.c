#include <sys/syscall.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>

#include "arm64_syscall.h"

/* code stolen from noir. Not re-entrant but should be ok since higher lock level is certainly taken */
#define PAGE_SIZE                       4096
#define PAGE_MASK                       (PAGE_SIZE - 1)
#define MIN_BRK_SIZE                    (16 * 1024 * 1024)

extern guest64_ptr startbrk_64;
static uint64_t curbrk;
static uint64_t mapbrk;

/* Reserve space so brk as a minimun guarantee space */
void arm64_setup_brk()
{
    //init phase
    if (!curbrk) {
        guest64_ptr map_result;

        curbrk = startbrk_64;
        mapbrk = ((startbrk_64 + PAGE_SIZE) & ~PAGE_MASK);
        map_result = mmap64_guest(mapbrk, MIN_BRK_SIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
        if (map_result == mapbrk) {
            mapbrk = mapbrk + MIN_BRK_SIZE;
        }
    }
}

long arm64_brk(struct arm64_target *context)
{
    guest64_ptr new_brk = context->regs.r[0];

    //sanity check
    if (new_brk < startbrk_64)
        goto out;
    if (new_brk) {
        // need to map new memory ?
        if (new_brk >= mapbrk) {
            guest64_ptr new_mapbrk = ((new_brk + PAGE_SIZE) & ~PAGE_MASK);
            guest64_ptr map_result;

            map_result = mmap64_guest(mapbrk, new_mapbrk - mapbrk, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
            if (map_result != mapbrk)
                goto out;
            mapbrk = new_mapbrk;
        }
        // init mem with zero
        if (new_brk > curbrk)
            memset(g_2_h_64(curbrk), 0, new_brk - curbrk);
    }

    curbrk = new_brk;
out:
    return curbrk;
}
