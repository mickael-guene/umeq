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

#define CACHE_WAY               4

struct cache_entry {
    uint64_t pc;
    void *ptr;
};

struct cache_info {
    int write_pos;
    int eject_pos;
    int clean;
    int reserved_0;
};

struct cache_config {
    int cache_entry_nb;
    int cache_entry_size;
    int jitter_area_size;
    int nb_of_pc_bit_to_drop;
};

struct internal_cache {
    struct internal_cache *next;
    struct cache cache;
    struct cache_config config;
    struct cache_info info;
    struct cache_entry *entry;
    char *area;
};

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
static void reset(struct internal_cache *acache)
{
    acache->info.write_pos = 0;
    acache->info.eject_pos = 0;
    acache->info.clean = 0;
    memset(acache->entry, 0, acache->config.cache_entry_size);
}

static void *lookup(struct cache *cache, uint64_t pc, int *cache_clean_event)
{
    struct internal_cache *acache = container_of(cache, struct internal_cache, cache);
    void *res = NULL;
    int entry_index = (pc >> acache->config.nb_of_pc_bit_to_drop) & (acache->config.cache_entry_nb - 1);
    int way;

    if (is_cache_clean_need) {
        ll_clean_caches(0, ~0);
        is_cache_clean_need = 0;
    }

    if (acache->info.clean) {
        reset(acache);
        *cache_clean_event = 1;
        return NULL;
    }

    for(way = 0; way < CACHE_WAY; way++) {
        if (acache->entry[way * acache->config.cache_entry_nb + entry_index].pc == pc)
            break;
    }
    if (way != CACHE_WAY)
        res = acache->entry[way * acache->config.cache_entry_nb + entry_index].ptr;

    return res;
}

static void *append(struct cache *cache, uint64_t pc, void *data, int size, int *cache_clean_event)
{
    struct internal_cache *acache = container_of(cache, struct internal_cache, cache);
    struct cache_entry *entry = NULL;
    int entry_index = (pc >> acache->config.nb_of_pc_bit_to_drop) & (acache->config.cache_entry_nb - 1);
    int way;

    /* handle jitter area full */
    if (acache->info.write_pos + size > acache->config.jitter_area_size) {
        reset(acache);
        *cache_clean_event = 1;
    }

    /* try to find a free way */
    for(way = 0; way < CACHE_WAY; way++) {
        if (acache->entry[way * acache->config.cache_entry_nb + entry_index].ptr == NULL)
            break;
    }
    /* if failed then eject an entry */
    if (way == CACHE_WAY)
        way = (acache->info.eject_pos++) & (CACHE_WAY - 1);
    entry = &acache->entry[way * acache->config.cache_entry_nb + entry_index];

    /* copy code and update entry */
    entry->pc = pc;
    entry->ptr = &acache->area[acache->info.write_pos];
    memcpy(entry->ptr, data, size);
    acache->info.write_pos += size;

    return entry->ptr;
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
    acache->config.nb_of_pc_bit_to_drop = nb_of_pc_bit_to_drop;
    /* we measure around 300 bytes per entry => we reserve around 1/16 of cache for entries */
    acache->config.cache_entry_nb = size / ( 16 * CACHE_WAY * sizeof(struct cache_entry));
    acache->config.cache_entry_size = CACHE_WAY * acache->config.cache_entry_nb * sizeof(struct cache_entry);
    acache->config.jitter_area_size = size - sizeof(struct internal_cache) - acache->config.cache_entry_size;
    acache->info.write_pos = 0;
    acache->info.eject_pos = 0;
    acache->info.clean = 0;
    acache->entry = (struct cache_entry *) (memory + sizeof(struct internal_cache));
    acache->area = (char *) (memory + sizeof(struct internal_cache) + acache->config.cache_entry_size);

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
