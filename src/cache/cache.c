/* This file is part of Umeq, an equivalent of qemu user mode emulation with improved robustness.
 *
 * Copyright (C) 2015 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>

#include "cache.h"

#define container_of(ptr, type, member) ({			\
    const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
    (type *)( (char *)__mptr - offsetof(type,member) );})

#define HASH_BIT_NB                     14
#define HASH_ENTRY_NB                   (1 << HASH_BIT_NB)
#define HASH_MASK                       (HASH_ENTRY_NB - 1)


struct cache_info {
    int write_pos;
    int clean;
};

struct cache_config {
    int jitter_area_size;
};

struct tb {
    uint16_t size;
    uint64_t guest_pc;
    struct tb *next_in_hash_list;
} __attribute__ ((packed));

struct internal_cache {
    struct internal_cache *next;
    struct cache cache;
    struct cache_config config;
    struct cache_info info;
    struct tb *cached[HASH_ENTRY_NB];
    struct tb *list[HASH_ENTRY_NB];
    char *area;
    void *area_end;
};

struct entry_header {
    uint16_t size;
    uint64_t guest_pc;
} __attribute__ ((packed));

static void *lookup(struct cache *cache, uint64_t pc, int *cache_clean_event);
static void *append(struct cache *cache, uint64_t pc, void *data, int size, int *cache_clean_event);

struct internal_cache *root = NULL;
static pthread_mutex_t ll_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned int is_cache_clean_need = 0;

/* link list functions */
static void ll_append_cache(struct internal_cache *cache)
{
    pthread_mutex_lock(&ll_mutex);
    cache->next = root;
    root = cache;
    pthread_mutex_unlock(&ll_mutex);
}

static void ll_remove_cache(struct internal_cache *cache)
{
    struct internal_cache *prev = NULL;
    struct internal_cache *current;

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

static void ll_clean_cache(struct internal_cache *cache, uint64_t from_pc, uint64_t to_pc_exclude)
{
    cache->info.clean = 1;
}

static void ll_clean_caches(uint64_t from_pc, uint64_t to_pc_exclude)
{
    struct internal_cache *current;

    pthread_mutex_lock(&ll_mutex);
    current = root;
    while(current) {
        ll_clean_cache(current, from_pc, to_pc_exclude);
        current = current->next;
    }

    pthread_mutex_unlock(&ll_mutex);
}

/* cache api */
static inline int hash_pc(uint64_t pc)
{
    return (pc >> 2) & HASH_MASK;
}

static inline void *tb_to_jit_area(struct tb *tb)
{
    return ((void *) tb) + sizeof(struct tb);
}

static void reset(struct internal_cache *acache)
{
    acache->info.write_pos = 0;
    acache->info.clean = 0;
    memset(acache->cached, 0, HASH_ENTRY_NB * sizeof(struct tb *));
    memset(acache->list, 0, HASH_ENTRY_NB * sizeof(struct tb *));
}

static void *lookup(struct cache *cache, uint64_t pc, int *cache_clean_event)
{
    struct internal_cache *acache = container_of(cache, struct internal_cache, cache);
    int hash = hash_pc(pc);
    struct tb *current;

    /* cache cleaning stuff */
    if (is_cache_clean_need) {
        ll_clean_caches(0, ~0);
        is_cache_clean_need = 0;
    }
    if (acache->info.clean) {
        reset(acache);
        *cache_clean_event = 1;
        return NULL;
    }

    /* first search in cache */
    current = acache->cached[hash];
    if (current && current->guest_pc == pc)
        return tb_to_jit_area(current);

    /* not found in cache so search in link list */
    current = acache->list[hash];
    while(current) {
        if (current->guest_pc == pc) {
            acache->cached[hash] = current;
            return tb_to_jit_area(current);
        }
        current = current->next_in_hash_list;
    }

    /* not found */
    return NULL;
}

static void *append(struct cache *cache, uint64_t pc, void *data, int size, int *cache_clean_event)
{
    struct internal_cache *acache = container_of(cache, struct internal_cache, cache);
    int hash = hash_pc(pc);
    struct tb *new_tb;
    void *res;

    /* handle jitter area full */
    if (acache->info.write_pos + size + sizeof(struct tb) > acache->config.jitter_area_size) {
        reset(acache);
        *cache_clean_event = 1;
    }

    /* setup new translation buffer */
    new_tb = (struct tb *) &acache->area[acache->info.write_pos];
    new_tb->size = size + sizeof(struct tb);
    new_tb->guest_pc = pc;
     /* insert at head */
    new_tb->next_in_hash_list = acache->list[hash];
    acache->list[hash] = new_tb;

    /* copy jit data */
    res = tb_to_jit_area(new_tb);
    memcpy(res, data, size);
    acache->info.write_pos += size + sizeof(struct tb);

    return res;
}

static uint64_t lookup_pc(struct cache *cache, void *host_pc, void **host_pc_start)
{
    struct internal_cache *acache = container_of(cache, struct internal_cache, cache);
    void *start_size_ptr = acache->area;
    uint64_t res = 0;

    do {
        struct tb *tb = (struct tb *) start_size_ptr;

        if (host_pc >= start_size_ptr && host_pc < start_size_ptr + tb->size) {
            res = tb->guest_pc;
            *host_pc_start = start_size_ptr + sizeof(struct tb);
            break;
        }
        start_size_ptr += tb->size;
    } while(start_size_ptr < acache->area_end);

    return res;
}

/* api */
struct cache *createCache(void *memory, int size, int nb_of_pc_bit_to_drop)
{
    struct internal_cache *acache;

    assert(memory);
    assert(size >= MIN_CACHE_SIZE);
    acache = (struct internal_cache *) memory;
    /* init acache */
    acache->next = NULL;
    acache->cache.lookup = lookup;
    acache->cache.append = append;
    acache->cache.lookup_pc = lookup_pc;
    acache->config.jitter_area_size = size - sizeof(struct internal_cache);
    acache->area = (char *) (memory + sizeof(struct internal_cache));
    acache->area_end = memory + size;
    reset(acache);

    ll_append_cache(acache);

    return &acache->cache;
}

void removeCache(struct cache *cache)
{
    struct internal_cache *acache = container_of(cache, struct internal_cache, cache);

    ll_remove_cache(acache);
}

void cleanCaches(uint64_t from_pc, uint64_t to_pc_exclude)
{
    /* FIXME : certainly better to use an add_atomic */
    is_cache_clean_need = 1;
}
