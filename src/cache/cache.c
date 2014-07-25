#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>

#include "cache.h"

#define container_of(ptr, type, member) ({			\
    const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
    (type *)( (char *)__mptr - offsetof(type,member) );})

#define ARM_CACHE_WAY           4
#define ARM_CACHE_ENTRY         8*1024

struct cache_entry {
    uint64_t pc;
    void *ptr;
};

struct cache_info {
    int write_pos;
    int eject_pos;
    int clean;
    int align;
};

#define ARM_JITTER_AREA_SIZE    (CACHE_SIZE - (ARM_CACHE_WAY * ARM_CACHE_ENTRY * sizeof(struct cache_entry)) - sizeof(struct cache) - sizeof(struct cache_info) -sizeof(struct arm_cache *))
struct arm_cache {
    struct arm_cache *next;
    struct cache cache;
    struct cache_info info;
    struct cache_entry entry[ARM_CACHE_WAY][ARM_CACHE_ENTRY];
    char area[ARM_JITTER_AREA_SIZE];
};

static void *lookup(struct cache *cache, uint64_t pc);
static void append(struct cache *cache, uint64_t pc, void *data, int size);

struct arm_cache *root = NULL;
static pthread_mutex_t ll_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned int is_cache_clean_need = 0;

/* link list functions */
static void ll_append_cache(struct arm_cache *cache)
{
    pthread_mutex_lock(&ll_mutex);
    cache->next = root;
    root = cache;
    pthread_mutex_unlock(&ll_mutex);
}

static void ll_remove_cache(struct arm_cache *cache)
{
    struct arm_cache *prev = NULL;
    struct arm_cache *current;

    pthread_mutex_lock(&ll_mutex);
    current = root;
    while(current) {
        if (current == cache) {
            if (prev)
                prev->next = current->next;
            else
                root = current->next;
            current->next = NULL;
            break;
        }
        prev = current;
        current = current->next;
    }
    assert(current != NULL);
    pthread_mutex_unlock(&ll_mutex);
}

static void ll_clean_cache(struct arm_cache *cache, uint64_t from_pc, uint64_t to_pc_exclude)
{
    cache->info.clean = 1;
}

static void ll_clean_caches(uint64_t from_pc, uint64_t to_pc_exclude)
{
    struct arm_cache *current;

    pthread_mutex_lock(&ll_mutex);
    current = root;
    while(current) {
        ll_clean_cache(current, from_pc, to_pc_exclude);
        current = current->next;
    }

    pthread_mutex_unlock(&ll_mutex);
}

/* cache api */
static void reset(struct arm_cache *acache)
{
    memset((void *)((uint64_t) acache + sizeof(struct arm_cache *)), 0, sizeof(*acache) - sizeof(struct arm_cache *));
    acache->cache.lookup = lookup;
    acache->cache.append = append;
}

static void *lookup(struct cache *cache, uint64_t pc)
{
    struct arm_cache *acache = container_of(cache, struct arm_cache, cache);
    void *res = NULL;
    int entry_index = (pc >> 2) & (ARM_CACHE_ENTRY - 1);
    int way;

    if (is_cache_clean_need) {
        ll_clean_caches(0, ~0);
        is_cache_clean_need = 0;
    }

    if (acache->info.clean)
        reset(acache);

    for(way = 0; way < ARM_CACHE_WAY; way++) {
        if (acache->entry[way][entry_index].pc == pc)
            break;
    }
    if (way != ARM_CACHE_WAY)
        res = acache->entry[way][entry_index].ptr;

    return res;
}

static void append(struct cache *cache, uint64_t pc, void *data, int size)
{
    struct arm_cache *acache = container_of(cache, struct arm_cache, cache);
    struct cache_entry *entry = NULL;
    int entry_index = (pc >> 2) & (ARM_CACHE_ENTRY - 1);
    int way;

    /* handle jitter area full */
    if (acache->info.write_pos + size > ARM_JITTER_AREA_SIZE)
        reset(acache);

    /* try to find a free way */
    for(way = 0; way < ARM_CACHE_WAY; way++) {
        if (acache->entry[way][entry_index].ptr == NULL)
            break;
    }
    /* if failed then eject an entry */
    if (way == ARM_CACHE_WAY)
        way = (acache->info.eject_pos++) & (ARM_CACHE_WAY - 1);
    entry = &acache->entry[way][entry_index];

    /* copy code and update entry */
    entry->pc = pc;
    entry->ptr = &acache->area[acache->info.write_pos];
    memcpy(entry->ptr, data, size);
    acache->info.write_pos += size;
}

/* api */
struct cache *createArmCache(void *memory)
{
    struct arm_cache *acache;

    assert(CACHE_SIZE >= sizeof(*acache));
    acache = (struct arm_cache *) memory;
    if (acache) {
        reset(acache);
        acache->next = NULL;
    }

    ll_append_cache(acache);

    return &acache->cache;
}

void removeArmCache(struct cache *cache)
{
    struct arm_cache *acache = container_of(cache, struct arm_cache, cache);

    ll_remove_cache(acache);
}

void cleanCaches(uint64_t from_pc, uint64_t to_pc_exclude)
{
    /* FIXME : certainly better to use an add_atomic */
    is_cache_clean_need = 1;
}

