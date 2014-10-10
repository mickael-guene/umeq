#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>

#include "arm_private.h"
#include "arm_syscall.h"
#include "runtime.h"

#define PAGE_SIZE                       4096
#define PAGE_MASK                       (PAGE_SIZE - 1)
#define PAGE_ALIGN_DOWN(addr)           ((addr) & ~PAGE_MASK)
#define PAGE_ALIGN_UP(addr)             (((addr) + PAGE_MASK) & ~PAGE_MASK)

enum vma_type {
    VMA_UNMAP = 0,
    VMA_MAP
};

struct vma_desc
{
    LIST_ENTRY(vma_desc) entries;
    enum vma_type type;
    uint32_t start_addr;
    uint32_t end_addr;//exclude
};
#define DESC_PER_PAGE  ((PAGE_SIZE) / sizeof(struct vma_desc))
LIST_HEAD(vma_head, vma_desc);

/* globals */
static int is_init = 0;
static struct vma_head free_vma_list = LIST_HEAD_INITIALIZER(free_vma_list);
static struct vma_head vma_list = LIST_HEAD_INITIALIZER(vma_list);
static pthread_mutex_t ll_big_mutex = PTHREAD_MUTEX_INITIALIZER;

static long mremap_syscall(void *old_address, size_t old_size, size_t new_size, int flags, void *new_address)
{
    return syscall(SYS_mremap, old_address, old_size, new_size, flags, new_address);
}

static int is_syscall_error(long res)
{
    if (res < 0 && res > -4096)
        return 1;
    else
        return 0;
}

/* this fucntion allocate a new bunch of free descriptors and add them to free_vma_list */
static void allocate_more_desc()
{
    int i;
    struct vma_desc *descs = (struct vma_desc *) mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(descs != MAP_FAILED);
    for(i = 0; i < DESC_PER_PAGE; i++)
        LIST_INSERT_HEAD(&free_vma_list, &descs[i], entries);
}

/* return a new free vma_desc */
static struct vma_desc *arm_get_free_desc()
{
    struct vma_desc *res = NULL;

    if (LIST_EMPTY(&free_vma_list))
        allocate_more_desc();
    assert(!LIST_EMPTY(&free_vma_list));
    res = LIST_FIRST(&free_vma_list);
    LIST_REMOVE(res, entries);

    assert(res != NULL);
    return res;
}

/* find the vma which is the addr owner */
static struct vma_desc *find_address_owner(uint32_t addr)
{
    struct vma_desc *desc;

    LIST_FOREACH(desc, &vma_list, entries) {
        if (addr >= desc->start_addr && addr < desc->end_addr)
            return desc;
    }
    fatal("Unable to find 0x%08x\n", addr);
}

/* insert a new area [start_addr, end_addr[ of type type. address are page align */
static void insert_area(uint32_t start_addr, uint32_t end_addr, enum vma_type type)
{
    struct vma_desc *first = find_address_owner(start_addr);
    struct vma_desc *last = find_address_owner(end_addr);

    if (first == last) {
        /* in that case area is inside a single vma */
        if (first->type == type) {
            /* nothing to do since area is already of type type */
            return ;
        } else {
            /* we need to split area in two or three */
            if (first->start_addr == start_addr) {
                struct vma_desc *mapone = arm_get_free_desc();

                /* fill descriptor value */
                mapone->type = type;
                mapone->start_addr = start_addr;
                mapone->end_addr = end_addr;
                first->start_addr = end_addr;
                /* insert new one */
                LIST_INSERT_BEFORE(first, mapone, entries);
            } else {
                struct vma_desc *mapone = arm_get_free_desc();
                struct vma_desc *unmapone = arm_get_free_desc();

                /* fill descriptor value */
                mapone->type = type;
                mapone->start_addr = start_addr;
                mapone->end_addr = end_addr;
                unmapone->type = 1 - type; /* FIXME: boohhhh */
                unmapone->start_addr = end_addr;
                unmapone->end_addr = first->end_addr;
                first->end_addr = mapone->start_addr;
                /* insert new ones */
                LIST_INSERT_AFTER(first, mapone, entries);
                LIST_INSERT_AFTER(mapone, unmapone, entries);
            }
        }
    } else {
        /* in that case area span multiples vma */
        struct vma_desc *current_mapped = NULL;
        struct vma_desc *current = first;
        struct vma_desc *next;

        while(current != last) {
            next = LIST_NEXT(current, entries);
            if (current_mapped) {
                //we remove current and extend current_mapped
                current_mapped-> end_addr = current->end_addr;
                LIST_REMOVE(current, entries);
                LIST_INSERT_HEAD(&free_vma_list, current, entries);
            } else {
                if (current->type == 1 - type) { /* FIXME: boohh again */
                    //shrink current and add a new map descriptor
                    current_mapped = arm_get_free_desc();
                    current_mapped->type = type;
                    current_mapped->start_addr = start_addr;
                    current_mapped->end_addr = current->end_addr;
                    current->end_addr = start_addr;
                    LIST_INSERT_AFTER(current, current_mapped, entries);
                } else {
                    current_mapped = current;
                }
            }
            current = next;
        }
        //ok we reach end of list, what to do with the last one
        if (last->type == 1 - type) { /* FIXME: boohh again */
            if (last->start_addr != end_addr) {
                /* we shrink last and increase current_mapped*/
                current_mapped->end_addr = end_addr;
                last->start_addr = end_addr;
            }
        } else {
            /* we extend current_mapped and remove last */
            current_mapped-> end_addr = last->end_addr;
            LIST_REMOVE(last, entries);
            LIST_INSERT_HEAD(&free_vma_list, last, entries);
        }
    }
    /* fixup this bad algo that can forget to merge prev block */
    {
        first = find_address_owner(start_addr);
        struct vma_desc *prev = NULL;
        struct vma_desc *desc;

        LIST_FOREACH(desc, &vma_list, entries) {
            if (LIST_NEXT(desc, entries) == first) {
                prev = desc;
                break;
            }
        }

        if (prev) {
            if ((prev)->type == type) {
                (prev)->end_addr = first->end_addr;
                LIST_REMOVE(first, entries);
                LIST_INSERT_HEAD(&free_vma_list, first, entries);
            }
        }
    }
}

static void dump_vma_list()
{
    struct vma_desc *desc;

    LIST_FOREACH(desc, &vma_list, entries) {
        if (desc->type == VMA_MAP)
            fprintf(stderr, "[0x%08x:0x%08x[ :   VMA_MAP\n", desc->start_addr, desc->end_addr);
        else
            fprintf(stderr, "[0x%08x:0x%08x[ : VMA_UNMAP\n", desc->start_addr, desc->end_addr);
    }
}

/* insert a new map area [start_addr, end_addr[. address are page align */
static void insert_map_area(uint32_t start_addr, uint32_t end_addr)
{
    insert_area(start_addr, end_addr, VMA_MAP);
}

/* insert a new unmap area */
static void insert_unmap_area(uint32_t start_addr, uint32_t end_addr)
{
    insert_area(start_addr, end_addr, VMA_UNMAP);
}

/* testing if an area is unmap */
static uint32_t is_area_unmap(uint32_t start_addr, uint32_t end_addr)
{
    struct vma_desc *owner;
    int res = 0;

    start_addr = PAGE_ALIGN_DOWN(start_addr);
    end_addr = PAGE_ALIGN_UP(end_addr);
    owner = find_address_owner(start_addr);

    if (owner->type == VMA_UNMAP && end_addr <= owner->end_addr)
        res = 1;
    else
        res = 0;

    return res;
}

/* find an unallocated of length bytes */
static uint32_t find_vma_with_hint(uint32_t length, uint32_t hint_addr)
{
    uint32_t res = 0;

    length = PAGE_ALIGN_UP(length);
    /* if we were given a hint address then try to allocate here */
    if (hint_addr) {
        struct vma_desc *owner;
        uint32_t end_addr;

        hint_addr = PAGE_ALIGN_DOWN(hint_addr);
        owner = find_address_owner(hint_addr);
        end_addr = PAGE_ALIGN_UP(hint_addr + length);

        if (owner->type == VMA_UNMAP && end_addr <= owner->end_addr) {
            insert_map_area(hint_addr, end_addr);
            res = hint_addr;
        }
    }
    /* if not already allocated using hint_addr then allocate somewhere else */
    if (!res) {
        struct vma_desc *desc;

        LIST_FOREACH(desc, &vma_list, entries) {
            uint32_t desc_length =  desc->end_addr - desc->start_addr;
            uint32_t start_addr = desc->start_addr;

            if (desc->type == VMA_UNMAP && desc_length >= length) {
                insert_map_area(desc->start_addr, desc->start_addr + length);
                res = start_addr;
                break;
            }
        }
    }

    return res;
}

/* init vma stuff */
static void arm_mmap_init()
{
    struct vma_desc *desc;
    uint64_t mmap_offset2;

    /* map 4 Gbytes of memory which is process user space */
#if 0
    mmap_offset = 0;
#else
    mmap_offset = (uint64_t) mmap((void *) (5 * 1024 * 1024 * 1024UL), 1 * 1024 * 1024 * 1024, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    assert(mmap_offset != (uint64_t) MAP_FAILED);
    mmap_offset2 = (uint64_t) mmap((void *) (mmap_offset + 1 * 1024 * 1024 * 1024), 1 * 1024 * 1024 * 1024, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    assert(mmap_offset2 != (uint64_t) MAP_FAILED);
    mmap_offset2 = (uint64_t) mmap((void *) (mmap_offset2 + 1 * 1024 * 1024 * 1024), 1 * 1024 * 1024 * 1024, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    assert(mmap_offset2 != (uint64_t) MAP_FAILED);
    mmap_offset2 = (uint64_t) mmap((void *) (mmap_offset2 + 1 * 1024 * 1024 * 1024), 1 * 1024 * 1024 * 1024, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    assert(mmap_offset2 != (uint64_t) MAP_FAILED);
#endif
    /* init vma_list */
    desc = arm_get_free_desc();
    desc->type = VMA_UNMAP;
    desc->start_addr = 0;
    desc->end_addr = 4 * 1024 * 1024 * 1024U - 4096;
    LIST_INSERT_HEAD(&vma_list, desc, entries);
    /* virtually map 32 first Kbytes with PROT_NONE to catch null pointer */
    insert_map_area(0, 32 * 1024);

    is_init = 1;
}

static int internal_mmap(uint32_t addr_p, uint32_t length_p, uint32_t prot_p,
                         uint32_t flags_p, uint32_t fd_p, uint32_t offset_p)
{
    void *res;

    if (!is_init)
        arm_mmap_init();
    {
        void *addr;
        size_t length = (size_t) length_p;
        int prot = (int) prot_p;
        int flags = (int) flags_p;
        int fd = (int) fd_p;
        off_t offset = (off_t) offset_p;

        if (addr_p == 0 || (flags & MAP_FIXED) == 0) {
            uint32_t start_addr = find_vma_with_hint(length, addr_p);

            addr = (void *) g_2_h(start_addr);
            flags |= MAP_FIXED;
        } else {
            uint32_t start_addr = addr_p;
            uint32_t end_addr = PAGE_ALIGN_UP(start_addr + length_p);
            
            addr = (void *) g_2_h(start_addr);
            insert_map_area(start_addr, end_addr);
        }

        res = mmap(addr, length, prot, flags, fd, offset);
    }

    return res == MAP_FAILED?-1:(int)(long)h_2_g(res);
}

static int internal_munmap(uint32_t addr_p, uint32_t length_p)
{
    int res;

    if (!is_init)
        arm_mmap_init();
    {
        uint32_t start_addr = addr_p;
        uint32_t end_addr = PAGE_ALIGN_UP(start_addr + length_p);
        void *addr = (void *) g_2_h(start_addr);
        size_t length = (size_t) length_p;

        res = munmap(addr, length);
        insert_unmap_area(start_addr, end_addr);
    }

    return res;
}

/* FIXME: this one need testing .... */
static int internal_mremap(uint32_t old_addr_p, uint32_t old_size_p, uint32_t new_size_p,
                           uint32_t flags_p, uint32_t new_addr_p)
{
    int res = -ENOMEM;
    long res_host;

    if (!is_init)
        arm_mmap_init();
    {
        void *old_addr = (void *) g_2_h(old_addr_p);
        size_t old_size = (size_t) old_size_p;
        size_t new_size = (size_t) new_size_p;
        int flags = (int) flags_p;
        void *new_addr = (void *) g_2_h(new_addr_p);
        uint32_t old_size_align_p = PAGE_ALIGN_UP(old_size_p);
        uint32_t new_size_align_p = PAGE_ALIGN_UP(new_size_p);

        if (flags & MREMAP_FIXED) {
            /* user request that we move mapping to new_addr */
            uint32_t start_addr = new_addr_p;
            uint32_t end_addr = PAGE_ALIGN_UP(new_addr_p + new_size_p);

            res_host = mremap_syscall(old_addr, old_size, new_size, flags, new_addr);
            if (!is_syscall_error(res_host)) {
                insert_map_area(start_addr, end_addr);
                res = new_addr_p;
            } else
                res = res_host;
        } else {
            if (new_size_align_p <= old_size_align_p) {
                //we shrink so we are sure to success and the kernel will not move segment
                res_host = mremap_syscall(old_addr, old_size, new_size, flags, new_addr);
                if (!is_syscall_error(res_host)) {
                    if (old_size_align_p != new_size_align_p) {
                        insert_unmap_area(old_addr_p + new_size_align_p, old_addr_p + old_size_align_p);
                    }
                    res = old_addr_p;
                } else
                    res = res_host;
            } else {
                //need to grow
                if (is_area_unmap(old_addr_p + old_size_align_p, old_addr_p + new_size_align_p)) {
                    /* be sure to remove MREMAP_MAYMOVE so following syscall either success with old addr or fail */
                    res_host = mremap_syscall(old_addr, old_size, new_size, (flags & ~MREMAP_MAYMOVE), new_addr);
                    if (!is_syscall_error(res_host)) {
                        insert_map_area(old_addr_p + old_size_align_p, old_addr_p + new_size_align_p);
                        res = old_addr_p;
                    } else
                        res = res_host;
                } else if (flags & MREMAP_MAYMOVE) {
                    //we need to move ....
                    uint32_t new_possible_addr_p = find_vma_with_hint(new_size_p, 0);
                    if (new_possible_addr_p) {
                        void * new_possible_addr = (void *) g_2_h(new_possible_addr_p);

                        res_host = mremap_syscall(old_addr, old_size, new_size, (flags | MREMAP_FIXED), new_possible_addr);
                        if (!is_syscall_error(res_host)) {
                            res = new_possible_addr_p;
                        } else {
                            insert_unmap_area(new_possible_addr_p, new_possible_addr_p + new_size_align_p);
                            res = res_host;
                        }
                    } else
                        res = -ENOMEM;
                } else
                    res = -ENOMEM;
            }
        }
    }

    return res;
}

/* public interface */
uint64_t mmap_offset = 0;

/* emulate arm syscall */
int arm_mmap(struct arm_target *context)
{
    int res;

    assert(context->is_in_signal == 0);
    pthread_mutex_lock(&ll_big_mutex);
    res = internal_mmap(context->regs.r[0], context->regs.r[1], context->regs.r[2],
                         context->regs.r[3], context->regs.r[4], context->regs.r[5]);
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}

/* emulate mmap2 syscall */
int arm_mmap2(struct arm_target *context)
{
    int res;

    assert(context->is_in_signal == 0);
    pthread_mutex_lock(&ll_big_mutex);
    res = internal_mmap(context->regs.r[0], context->regs.r[1], context->regs.r[2],
                         context->regs.r[3], context->regs.r[4], context->regs.r[5] * 4096);
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}

/* emulate munmap syscall */
int arm_munmap(struct arm_target *context)
{
    int res;

    assert(context->is_in_signal == 0);
    pthread_mutex_lock(&ll_big_mutex);
    res = internal_munmap(context->regs.r[0], context->regs.r[1]);
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}

/* emulate mremap syscall */
int arm_mremap(struct arm_target *context)
{
    int res;

    assert(context->is_in_signal == 0);
    pthread_mutex_lock(&ll_big_mutex);
    res = internal_mremap(context->regs.r[0], context->regs.r[1], context->regs.r[2],
                          context->regs.r[3], context->regs.r[4]);
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}

/* allocate memory in guest memory space and return guest address */
guest_ptr mmap_guest(guest_ptr addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    uint32_t res;

    pthread_mutex_lock(&ll_big_mutex);
    res = internal_mmap(addr, length, prot, flags, fd, offset);
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}

int munmap_guest(guest_ptr addr, size_t length)
{
    int res;

    pthread_mutex_lock(&ll_big_mutex);
    res = internal_munmap(addr, length);
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}
