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

#define PAGE_SIZE                       4096
#define PAGE_MASK                       (PAGE_SIZE - 1)
#define PAGE_ALIGN_DOWN(addr)           ((addr) & ~PAGE_MASK)
#define PAGE_ALIGN_UP(addr)             (((addr) + PAGE_MASK) & ~PAGE_MASK)
#define KERNEL_CHOOSE_START_ADDR        0x4000000000UL
#define ENOMEM_64                       ((uint64_t) -ENOMEM)
#define MAX(a,b)                        ((a)>=(b)?(a):(b))
#define MIN(a,b)                        ((a)<(b)?(a):(b))

enum vma_type {
    VMA_UNMAP = 0,
    VMA_MAP
};

struct vma_desc
{
    LIST_ENTRY(vma_desc) entries;
    enum vma_type type;
    uint64_t start_addr;
    uint64_t end_addr;//exclude
};
#define DESC_PER_PAGE  ((PAGE_SIZE) / sizeof(struct vma_desc))
LIST_HEAD(vma_head, vma_desc);

struct shmat_desc
{
    LIST_ENTRY(shmat_desc) entries;
    uint64_t start_addr;
    uint64_t end_addr;
};
#define SHMAT_DESC_PER_PAGE  ((PAGE_SIZE) / sizeof(struct shmat_desc))
LIST_HEAD(shmat_head, shmat_desc);

/* globals */
static int is_init = 0;
static struct vma_head free_vma_list = LIST_HEAD_INITIALIZER(free_vma_list);
static struct vma_head vma_list = LIST_HEAD_INITIALIZER(vma_list);
static pthread_mutex_t ll_big_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct shmat_head free_shmat_list = LIST_HEAD_INITIALIZER(free_shmat_list);
static struct shmat_head shmat_list = LIST_HEAD_INITIALIZER(shmat_list);

static long mremap_syscall(void *old_address, size_t old_size, size_t new_size, int flags, void *new_address)
{
    return syscall(SYS_mremap, old_address, old_size, new_size, flags, new_address);
}

static long mmap_syscall(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    return syscall(SYS_mmap, addr, length, prot, flags, fd, offset);
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
    struct vma_desc *descs = (struct vma_desc *) mmap_syscall(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(is_syscall_error((long) descs) == 0);
    for(i = 0; i < DESC_PER_PAGE; i++)
        LIST_INSERT_HEAD(&free_vma_list, &descs[i], entries);
}

/* return a new free vma_desc */
static struct vma_desc *arm64_get_free_desc()
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
static struct vma_desc *find_address_owner_without_assert(uint64_t addr)
{
    struct vma_desc *desc = NULL;

    LIST_FOREACH(desc, &vma_list, entries) {
        if (addr >= desc->start_addr && addr < desc->end_addr)
            return desc;
    }
    
    return desc;
}

static struct vma_desc *find_address_owner(uint64_t addr)
{
    struct vma_desc *desc = find_address_owner_without_assert(addr);

    if (desc)
        return desc;
    else
        fatal("Unable to find 0x%016lx\n", addr);
}

/* insert a new area [start_addr, end_addr[ of type type. address are page align */
static void insert_area(uint64_t start_addr, uint64_t end_addr, enum vma_type type)
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
                struct vma_desc *mapone = arm64_get_free_desc();

                /* fill descriptor value */
                mapone->type = type;
                mapone->start_addr = start_addr;
                mapone->end_addr = end_addr;
                first->start_addr = end_addr;
                /* insert new one */
                LIST_INSERT_BEFORE(first, mapone, entries);
            } else {
                struct vma_desc *mapone = arm64_get_free_desc();
                struct vma_desc *unmapone = arm64_get_free_desc();

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
                    current_mapped = arm64_get_free_desc();
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

#if 0
static void dump_vma_list()
{
    struct vma_desc *desc;

    LIST_FOREACH(desc, &vma_list, entries) {
        if (desc->type == VMA_MAP)
            fprintf(stderr, "[0x%016lx:0x%016lx[ :   VMA_MAP\n", desc->start_addr, desc->end_addr);
        else
            fprintf(stderr, "[0x%016lx:0x%016lx[ : VMA_UNMAP\n", desc->start_addr, desc->end_addr);
    }
}
#endif

/* insert a new map area [start_addr, end_addr[. address are page align */
static void insert_map_area(uint64_t start_addr, uint64_t end_addr)
{
    insert_area(start_addr, end_addr, VMA_MAP);
}

/* insert a new unmap area */
static void insert_unmap_area(uint64_t start_addr, uint64_t end_addr)
{
    insert_area(start_addr, end_addr, VMA_UNMAP);
}

/* testing if an area is unmap */
static uint32_t is_area_unmap(uint64_t start_addr, uint64_t end_addr)
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

/* testing if a range of address is map */
static int is_area_valid(uint64_t start_addr, uint64_t end_addr)
{
    struct vma_desc *owner;
    int res = 0;

    start_addr = PAGE_ALIGN_DOWN(start_addr);
    end_addr = PAGE_ALIGN_UP(end_addr);
    owner = find_address_owner_without_assert(start_addr);
    if (owner && owner->type == VMA_MAP && end_addr <= owner->end_addr)
        res = 1;

    return res;
}

/* find an unallocated of length bytes */
static uint64_t find_vma(uint64_t length)
{
    struct vma_desc *desc;
    uint64_t res = ENOMEM_64;

    length = PAGE_ALIGN_UP(length);
    LIST_FOREACH(desc, &vma_list, entries) {
        uint64_t desc_length =  desc->end_addr - desc->start_addr;
        uint64_t start_addr = desc->start_addr;

        if (desc->type == VMA_UNMAP && desc_length >= length) {
            insert_map_area(desc->start_addr, desc->start_addr + length);
            res = start_addr;
            break;
        }
    }

    return res;
}

/* find an unallocated of length bytes with kernel choose address */
static uint64_t find_vma_with_kernel_choose(uint64_t length)
{
    uint64_t res = ENOMEM_64;
    struct vma_desc *desc;

    length = PAGE_ALIGN_UP(length);
    /* start the search from KERNEL_CHOOSE_START_ADDR */
    desc = find_address_owner(KERNEL_CHOOSE_START_ADDR);
    while(desc) {
        if (desc->type == VMA_UNMAP) {
            uint64_t start_addr = MAX(desc->start_addr, KERNEL_CHOOSE_START_ADDR);
            uint64_t available_space =  desc->end_addr - start_addr;

            if (available_space >= length) {
                insert_map_area(start_addr, start_addr + length);
                res = start_addr;
                break;
            }
        }
        desc = LIST_NEXT(desc, entries);
    }

    /* in case we don't find free space we search from all descriptor */
    if (res == ENOMEM_64)
        res = find_vma(length);

    return res;
}

/* find an unallocated of length bytes */
static uint64_t find_vma_with_hint(uint64_t length, uint64_t hint_addr)
{
    uint64_t res = ENOMEM_64;

    length = PAGE_ALIGN_UP(length);
    /* if we were given a hint address then try to allocate here */
    if (hint_addr) {
        struct vma_desc *owner;
        uint64_t end_addr;

        hint_addr = PAGE_ALIGN_DOWN(hint_addr);
        owner = find_address_owner(hint_addr);
        end_addr = PAGE_ALIGN_UP(hint_addr + length);

        if (owner->type == VMA_UNMAP && end_addr <= owner->end_addr) {
            insert_map_area(hint_addr, end_addr);
            res = hint_addr;
        }
    }
    /* if not already allocated using hint_addr then allocate somewhere else */
    if (res == ENOMEM_64)
        res = find_vma(length);

    return res;
}

/* shmat desc helper functions */
static void allocate_more_shmat_desc()
{
    int i;
    struct shmat_desc *descs = (struct shmat_desc *) mmap_syscall(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(is_syscall_error((long) descs) == 0);
    for(i = 0; i < SHMAT_DESC_PER_PAGE; i++)
        LIST_INSERT_HEAD(&free_shmat_list, &descs[i], entries);
}

static struct shmat_desc *get_shmat_free_desc()
{
    struct shmat_desc *res = NULL;

    if (LIST_EMPTY(&free_shmat_list))
        allocate_more_shmat_desc();
    assert(!LIST_EMPTY(&free_shmat_list));
    res = LIST_FIRST(&free_shmat_list);
    LIST_REMOVE(res, entries);

    assert(res != NULL);
    return res;
}

static struct shmat_desc *shm_find(uint64_t start_addr)
{
    struct shmat_desc *desc;

    LIST_FOREACH(desc, &shmat_list, entries) {
        if (start_addr == desc->start_addr) {
            return desc;
        }
    }

    return NULL;
}

static void shm_insert(uint64_t start_addr, uint64_t end_addr)
{
    struct shmat_desc *desc = get_shmat_free_desc();

    desc->start_addr = start_addr;
    desc->end_addr = end_addr;

    LIST_INSERT_HEAD(&shmat_list, desc, entries);
}

static void shm_remove(uint64_t start_addr)
{
    struct shmat_desc *desc = shm_find(start_addr);

    if (desc) {
        insert_unmap_area(desc->start_addr, desc->end_addr);
        LIST_REMOVE(desc, entries);
        LIST_INSERT_HEAD(&free_shmat_list, desc, entries);
    } else
        warning("Unable to find 0x%016lx for shmdt\n", start_addr);
}

/* helpers for /proc/self/maps */
static int conv_hex_to_int(int c)
{
    int res;

    switch(c) {
        case 48 ... 57:
            res = c - 48;
            break;
        case 97 ... 102:
            res = c - 97 + 10;
            break;
        case 65 ... 70:
            res = c - 65 + 10;
            break;
        default:
            fatal("invalid char %c\n", c);
    }

    return res;
}

static char *split_line(char *line, uint64_t *start_addr_p, uint64_t *end_addr_p)
{
    uint64_t start_addr = 0;
    uint64_t end_addr = 0;
    unsigned char c;
    int i = 0;
    int len = strlen(line);
    int is_in_start_addr = 1;
    char *res = NULL;

    while(i < len) {
        c = line[i++];
        if (is_in_start_addr) {
            if (c == '-') {
                is_in_start_addr = 0;
            } else {
                start_addr = start_addr * 16 + conv_hex_to_int(c);
            }
        } else {
            if (c == ' ') {
                res = &line[i];
                break;
            } else {
                end_addr = end_addr * 16 + conv_hex_to_int(c);
            }
        }
    }

    *start_addr_p = start_addr;
    *end_addr_p = end_addr;

    return res;
}

static int getline_internal(char *line, int max_length, int fd)
{
    char c;
    int i = 0;

    while(i<max_length - 1 && read(fd, &c, 1) > 0) {
        line[i++] = c;
        if (c == '\n')
            break;
    }
    line[i] = '\0';

    return i==0?-1:i;
}

static int is_none_prot_area(char *line)
{
    if (line[0] == '-' && line [1] == '-' && line[2] == '-')
        return 1;
    else
        return 0;
}

static int is_overlap(uint64_t start_1, uint64_t end_1, uint64_t start_2, uint64_t end_2)
{
    if (start_2 >= start_1 && start_2 < end_1)
        return 1;
    if (end_2 - 1 >= start_1 && end_2 - 1 < end_1)
        return 1;
    return 0;
}

static void get_overlap_area(uint64_t start_1, uint64_t end_1, uint64_t start_2, uint64_t end_2, uint64_t *start, uint64_t *end)
{
    *start = MAX(start_1, start_2);
    *end = MIN(end_1, end_2);
}

static void handle_none_prot_area(int fd_res, char *line_with_remove_addresses, uint64_t start_addr, uint64_t end_addr)
{
    struct vma_desc *owner = find_address_owner(start_addr);

    while(is_overlap(owner->start_addr, owner->end_addr, start_addr, end_addr)) {
        if (owner->type == VMA_MAP) {
            char new_line[512];
            uint64_t start_overlap_area;
            uint64_t end_overlap_area;

            get_overlap_area(owner->start_addr, owner->end_addr, start_addr, end_addr, &start_overlap_area, &end_overlap_area);
            sprintf(new_line, "%08lx-%08lx %s", start_overlap_area, end_overlap_area, line_with_remove_addresses);
            write(fd_res, new_line, strlen(new_line));
        }
        owner = LIST_NEXT(owner, entries);
    }
}

/* init vma stuff */
static void arm64_mmap_init()
{
    struct vma_desc *desc;
    long res;

    /* This prevent further umeq internal mmap to allocate in guest memory with a direct mmap syscall */
    res = mmap_syscall((void *) 0x400000, 512 * 1024 * 1024 * 1024UL - 0x400000, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(!is_syscall_error(res));
    assert(res == 0x400000);
    /* init vma_list */
    desc = arm64_get_free_desc();
    desc->type = VMA_UNMAP;
    /* FIXME: read /proc/sys/vm/mmap_min_addr to set start_addr */
    desc->start_addr = 0x400000;
    desc->end_addr = 512 * 1024 * 1024 * 1024UL;
    LIST_INSERT_HEAD(&vma_list, desc, entries);

    is_init = 1;
}

static long internal_mmap(uint64_t addr_p, uint64_t length_p, uint64_t prot_p,
                         uint64_t flags_p, uint64_t fd_p, uint64_t offset_p)
{
    long res;
    uint64_t res_vma = ENOMEM_64;

    if (!is_init)
        arm64_mmap_init();
    {
        void *addr;
        size_t length = (size_t) length_p;
        int prot = (int) prot_p;
        int flags = (int) flags_p;
        int fd = (int) fd_p;
        off_t offset = (off_t) offset_p;

        /* first find and insert vma */
        if (addr_p == 0) {
            res_vma = find_vma_with_kernel_choose(length);

            addr = (void *) g_2_h(res_vma);
        } else if ((flags & MAP_FIXED) == 0) {
            res_vma = find_vma_with_hint(length, addr_p);

            addr = (void *) g_2_h(res_vma);
        } else {
            res_vma = addr_p;
            uint64_t end_addr = PAGE_ALIGN_UP(res_vma + length_p);
            
            addr = (void *) g_2_h(res_vma);
            insert_map_area(res_vma, end_addr);
        }

        /* if we find suitable memory area then try to map it */
        if (res_vma != ENOMEM_64) {
            res = mmap_syscall(addr, length, prot, flags | MAP_FIXED, fd, offset);
            if (is_syscall_error(res)) {
                uint64_t end_addr = PAGE_ALIGN_UP(res_vma + length_p);

                insert_unmap_area(res_vma, end_addr);
            }
        } else
            res = res_vma;
    }

    return is_syscall_error(res)?res:h_2_g(res);
}

static long internal_munmap(uint64_t addr_p, uint64_t length_p)
{
    long res;

    if (!is_init)
        arm64_mmap_init();
    {
        uint64_t start_addr = addr_p;
        uint64_t end_addr = PAGE_ALIGN_UP(start_addr + length_p);
        void *addr = (void *) g_2_h(start_addr);
        size_t length = (size_t) length_p;

        res = munmap(addr, length);
        if (!is_syscall_error(res))
            insert_unmap_area(start_addr, end_addr);
    }

    return res;
}

/* FIXME: this one need testing .... */
static long internal_mremap(uint64_t old_addr_p, uint64_t old_size_p, uint64_t new_size_p,
                           uint64_t flags_p, uint64_t new_addr_p)
{
    long res = -ENOMEM;
    long res_host;

    if (!is_init)
        arm64_mmap_init();
    if (PAGE_ALIGN_DOWN(old_addr_p) != old_addr_p) {
        res = -EINVAL;
    } else if (!is_area_valid(old_addr_p, old_addr_p + old_size_p)) {
        res = -EFAULT;
    } else {
        void *old_addr = (void *) g_2_h(old_addr_p);
        size_t old_size = (size_t) old_size_p;
        size_t new_size = (size_t) new_size_p;
        int flags = (int) flags_p;
        void *new_addr = (void *) g_2_h(new_addr_p);
        uint64_t old_size_align_p = PAGE_ALIGN_UP(old_size_p);
        uint64_t new_size_align_p = PAGE_ALIGN_UP(new_size_p);


        if (flags & MREMAP_FIXED) {
            /* user request that we move mapping to new_addr */
            uint64_t start_addr = new_addr_p;
            uint64_t end_addr = PAGE_ALIGN_UP(new_addr_p + new_size_p);

            res_host = mremap_syscall(old_addr, old_size, new_size, flags, new_addr);
            if (!is_syscall_error(res_host)) {
                uint64_t old_start_addr = old_addr_p;
                uint64_t old_end_addr = PAGE_ALIGN_UP(old_addr_p + old_size_p);

                insert_unmap_area(old_start_addr, old_end_addr);
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
                }
                /* if we can move and not enought space of previous move try fail we try to move */
                if ((flags & MREMAP_MAYMOVE) && is_syscall_error(res)) {
                    //we need to move ....
                    uint64_t new_possible_addr_p = find_vma_with_kernel_choose(new_size_p);
                    if (new_possible_addr_p != ENOMEM_64) {
                        void * new_possible_addr = (void *) g_2_h(new_possible_addr_p);

                        res_host = mremap_syscall(old_addr, old_size, new_size, (flags | MREMAP_FIXED), new_possible_addr);
                        if (!is_syscall_error(res_host)) {
                            insert_unmap_area(old_addr_p, old_addr_p + old_size_p);
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

static long internal_shmat(uint64_t shmid_p, uint64_t shmaddr_p, uint64_t shmflg_p)
{
    long res;
    int shmid = (int) shmid_p;
    void *shmaddr;
    int shmflg = (int) shmflg_p;
    struct shmid_ds shm_info;
    uint64_t start_addr = 0;
    uint64_t end_addr = 0;
    uint64_t length;

    /* first find segment size */
    res = syscall(SYS_shmctl, shmid, IPC_STAT, &shm_info);
    if (is_syscall_error(res))
        return res;
    length = PAGE_ALIGN_UP(shm_info.shm_segsz);

    /* map vma memory area */
    if (shmaddr_p == 0) {
        start_addr = find_vma_with_kernel_choose(length);
        end_addr = start_addr + length;
    } else {
        start_addr = PAGE_ALIGN_DOWN(shmaddr_p);
        end_addr = start_addr + length;
        insert_map_area(start_addr, end_addr);
    }

    /* do the shmat */
    if (start_addr != ENOMEM_64) {
        shmaddr = g_2_h(start_addr);
        res = syscall(SYS_shmat, shmid, shmaddr, shmflg);
        if (is_syscall_error(res))
            insert_unmap_area(start_addr, end_addr);
        else
            shm_insert(start_addr, end_addr);
    } else {
        res = -ENOMEM;
    }

    return is_syscall_error(res)?res:h_2_g(res);
}

static long internal_shmdt(uint64_t shmaddr_p)
{
    long res;

    res = syscall(SYS_shmdt, g_2_h(shmaddr_p));
    if (!is_syscall_error(res))
        shm_remove(shmaddr_p);

    return res;
}

static void internal_write_offset_proc_self_maps(int fd_orig, int fd_res)
{
    char line[512];
    char *line_with_remove_addresses;
    uint64_t start_addr;
    uint64_t end_addr;

    while (getline_internal(line, sizeof(line), fd_orig) != -1) {
        line_with_remove_addresses = split_line(line, &start_addr, &end_addr);
        if ((start_addr >= 0x400000) && (end_addr < 512 * 1024 * 1024 * 1024UL)) {
            if (is_none_prot_area(line_with_remove_addresses)) {
                handle_none_prot_area(fd_res, line_with_remove_addresses, start_addr, end_addr);
            } else {
                char new_line[512];
                guest_ptr start_addr_guest = h_2_g(start_addr);
                guest_ptr end_addr_guest = h_2_g(end_addr);

                sprintf(new_line, "%08lx-%08lx %s", start_addr_guest, end_addr_guest, line_with_remove_addresses);
                write(fd_res, new_line, strlen(new_line));
            }
        }
    }
    lseek(fd_res, 0, SEEK_SET);
}

/* public interface */
/* emulate arm64 syscall */
long arm64_mmap(struct arm64_target *context)
{
    long res;

    assert(context->is_in_signal == 0);
    pthread_mutex_lock(&ll_big_mutex);
    res = internal_mmap(context->regs.r[0], context->regs.r[1], context->regs.r[2],
                         context->regs.r[3], context->regs.r[4], context->regs.r[5]);
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}

/* emulate munmap syscall */
long arm64_munmap(struct arm64_target *context)
{
    long res;

    if (context->is_in_signal) {
        /* we leak vma space but at least we unmaep memory */
        res = munmap(g_2_h(context->regs.r[0]), context->regs.r[1]);
    } else {
        pthread_mutex_lock(&ll_big_mutex);
        res = internal_munmap(context->regs.r[0], context->regs.r[1]);
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
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}

/* allocate memory in guest memory space and return guest address */
guest_ptr mmap_guest(guest_ptr addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    long res;

    pthread_mutex_lock(&ll_big_mutex);
    res = internal_mmap(addr, length, prot, flags, fd, offset);
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}

int munmap_guest(guest_ptr addr, size_t length)
{
    long res;

    pthread_mutex_lock(&ll_big_mutex);
    res = internal_munmap(addr, length);
    pthread_mutex_unlock(&ll_big_mutex);

    return res;
}

/* use original /proc/self/maps + internal info to write a correct one */
void write_offset_proc_self_maps(int fd_orig, int fd_res)
{
    pthread_mutex_lock(&ll_big_mutex);
    internal_write_offset_proc_self_maps(fd_orig, fd_res);
    pthread_mutex_unlock(&ll_big_mutex);
}
