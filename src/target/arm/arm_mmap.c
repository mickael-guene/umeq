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

#include "target32.h"
#include "arm_private.h"
#include "runtime.h"
#include "cache.h"

#define MAPPING_START                   0x8000
#define KERNEL_CHOOSE_START_ADDR        0x40000000
#define MAPPING_RESERVE_IN_SIGNAL_START 0xc0000000
#define MAPPING_RESERVE_IN_SIGNAL_END   0xf0000000
#define MAPPING_END                     0xfffff000

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
    desc->start_addr = MAPPING_START;
    desc->end_addr = MAPPING_RESERVE_IN_SIGNAL_START;
    LIST_INSERT_HEAD(&vma_list, desc, entries);
    desc = arm64_get_free_desc();
    desc->type = VMA_UNMAP;
    desc->start_addr = MAPPING_RESERVE_IN_SIGNAL_END;
    desc->end_addr = MAPPING_END;
    LIST_INSERT_HEAD(&vma_list, desc, entries);

    /* map at fix address since this make ptrace emulation easier */
    mmap_offset = 5 * 1024 * 1024 * 1024UL;

    is_init = 1;
}

/* public interface */
uint64_t mmap_offset = 0;
/* emulate mmap syscall */
int arm_mmap(struct arm_target *context)
{
    int res;

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

/* emulate mmap2 syscall */
int arm_mmap2(struct arm_target *context)
{
    int res;

    if (context->is_in_signal) {
        res = internal_mmap_signal(context->regs.r[0], context->regs.r[1], context->regs.r[2],
                                   context->regs.r[3], context->regs.r[4], context->regs.r[5] * 4096);
    } else {
        assert(context->is_in_signal == 0);
        pthread_mutex_lock(&ll_big_mutex);
        res = internal_mmap(context->regs.r[0], context->regs.r[1], context->regs.r[2],
                             context->regs.r[3], context->regs.r[4], context->regs.r[5] * 4096);
        if (!desc_next_memory)
            allocate_more_desc_memory();
        pthread_mutex_unlock(&ll_big_mutex);
    }

    return res;
}

/* emulate munmap syscall */
int arm_munmap(struct arm_target *context)
{
    int res;

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
int arm_mremap(struct arm_target *context)
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
int arm_shmat(struct arm_target *context)
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
int arm_shmdt(struct arm_target *context)
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
