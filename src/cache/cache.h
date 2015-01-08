#include <stdlib.h>
#include "jitter_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __CACHE__
#define __CACHE__ 1

#define MIN_CACHE_SIZE  (1 * 1024 * 1024)

struct cache {
    void *(*lookup)(struct cache *cache, uint64_t pc);
    void (*append)(struct cache *cache, uint64_t pc, void *data, int size);
};

struct cache *createCache(void *memory, int size);
void removeCache(struct cache *cache);
void cleanCaches(uint64_t from_pc, uint64_t to_pc_exclude);

#endif

#ifdef __cplusplus
}
#endif

