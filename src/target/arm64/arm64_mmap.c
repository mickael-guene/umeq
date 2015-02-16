#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <stdio.h>

#include "target64.h"
#include "arm64_private.h"
#include "runtime.h"
#include "cache.h"

/* mozilla allocator take arm64 page size for computation => 64K pages.
 * Unfortunaly when real size pages are 4K it can lead to wrong computations
 * which can lead in some cases to allocate less memory than needed.
 * So to workaround this bug we align kernel_cursor on FORCE_ALIGNMENT value.
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1124580
*/
#define FORCE_ALIGNMENT                 (64 * 1024UL)

#define MAPPING_START                   0x0000400000
#define KERNEL_CHOOSE_START_ADDR        0x4000000000UL
#define MAPPING_RESERVE_IN_SIGNAL_START 0x7000000000UL
#define MAPPING_RESERVE_IN_SIGNAL_END   0x8000000000UL
#define MAPPING_END                     0x8000000000UL

#define PAGE_SIZE                       4096

/* include mmap common code */
#include "../memory/mmap_common.c"

/* init vma stuff */
static void mmap_init()
{
    struct vma_desc *desc;
    int i;

    /* init bootstrap vma list */
    desc = (struct vma_desc *) desc_bootstrap_memory;
    for(i = 0; i < DESC_PER_PAGE; i++)
        LIST_INSERT_HEAD(&free_vma_list, &desc[i], entries);
    /* init vma_list */
    desc = arm64_get_free_desc();
    desc->type = VMA_UNMAP;
    /* FIXME: read /proc/sys/vm/mmap_min_addr to set start_addr */
    desc->start_addr = MAPPING_START;
    desc->end_addr = MAPPING_RESERVE_IN_SIGNAL_START;
    LIST_INSERT_HEAD(&vma_list, desc, entries);

    is_init = 1;
}

/* public interface */
/* emulate arm64 syscall */
long arm64_mmap(struct arm64_target *context)
{
    long res;

    if (context->is_in_signal) {
        res = internal_mmap_signal(context->regs.r[0], context->regs.r[1], context->regs.r[2],
                                   context->regs.r[3], context->regs.r[4], context->regs.r[5]);
    } else {
        assert(context->is_in_signal == 0);
        pthread_mutex_lock(&ll_big_mutex);
        res = internal_mmap(context->regs.r[0], context->regs.r[1], context->regs.r[2],
                             context->regs.r[3], context->regs.r[4], context->regs.r[5]);
        if (!desc_next_memory)
            allocate_more_desc_memory();
        pthread_mutex_unlock(&ll_big_mutex);
    }

    return res;
}

/* emulate munmap syscall */
long arm64_munmap(struct arm64_target *context)
{
    long res;

    if (context->is_in_signal) {
        /* we may leak vma space but at least we unmap memory */
        res = munmap(g_2_h(context->regs.r[0]), context->regs.r[1]);
    } else {
        pthread_mutex_lock(&ll_big_mutex);
        res = internal_munmap(context->regs.r[0], context->regs.r[1]);
        if (!desc_next_memory)
            allocate_more_desc_memory();
        pthread_mutex_unlock(&ll_big_mutex);
    }

    return res;
}

/* emulate mremap syscall */
long arm64_mremap(struct arm64_target *context)
{
    long res;

    assert(context->is_in_signal == 0);
    pthread_mutex_lock(&ll_big_mutex);
    res = internal_mremap(context->regs.r[0], context->regs.r[1], context->regs.r[2],
                          context->regs.r[3], context->regs.r[4]);
    if (!desc_next_memory)
        allocate_more_desc_memory();
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}

/* emulate shmat */
long arm64_shmat(struct arm64_target *context)
{
    long res;

    assert(context->is_in_signal == 0);
    pthread_mutex_lock(&ll_big_mutex);
    res = internal_shmat(context->regs.r[0], context->regs.r[1], context->regs.r[2]);
    if (!desc_next_memory)
        allocate_more_desc_memory();
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}

/* emulate shmdt */
long arm64_shmdt(struct arm64_target *context)
{
    long res;

    assert(context->is_in_signal == 0);
    pthread_mutex_lock(&ll_big_mutex);
    res = internal_shmdt(context->regs.r[0]);
    if (!desc_next_memory)
        allocate_more_desc_memory();
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}

/* allocate memory in guest memory space and return guest address */
guest_ptr mmap_guest(guest_ptr addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    long res;

    pthread_mutex_lock(&ll_big_mutex);
    res = internal_mmap(addr, length, prot, flags, fd, offset);
    if (!desc_next_memory)
        allocate_more_desc_memory();
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}

int munmap_guest(guest_ptr addr, size_t length)
{
    long res;

    pthread_mutex_lock(&ll_big_mutex);
    res = internal_munmap(addr, length);
    if (!desc_next_memory)
        allocate_more_desc_memory();
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}

void *munmap_guest_ongoing(guest_ptr addr, size_t length)
{
    void *res;

    pthread_mutex_lock(&ll_big_mutex);
    res = internal_munmap_ongoing(addr, length);
    if (!desc_next_memory)
        allocate_more_desc_memory();
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}
