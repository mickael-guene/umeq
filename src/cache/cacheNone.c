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
#include <stdio.h>

#include "cache.h"

#define container_of(ptr, type, member) ({          \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})

struct internal_cache {
    struct cache cache;
    void *data;
    uint64_t pc;
};

static void *lookup_none(struct cache *cache, uint64_t pc, int *cache_clean_event)
{
    *cache_clean_event = 1;

    return NULL;
}

static void *append_none(struct cache *cache, uint64_t pc, void *data, int size, int *cache_clean_event)
{
    struct internal_cache *acache = container_of(cache, struct internal_cache, cache);

    acache->data = data;
    acache->pc = pc;

    return data;
}

static uint64_t lookup_pc_none(struct cache *cache, void *host_pc, void **host_pc_start)
{
    struct internal_cache *acache = container_of(cache, struct internal_cache, cache);

    *host_pc_start = acache->data;

    return acache->pc;
}

/* api */
struct cache *createCacheNone(void *memory, int size)
{
    struct internal_cache *acache;

    assert(memory);
    assert(size >= MIN_CACHE_SIZE_NONE);
    acache = (struct internal_cache *) memory;
    /* init acache */
    acache->cache.lookup = lookup_none;
    acache->cache.append = append_none;
    acache->cache.lookup_pc = lookup_pc_none;
    acache->data = NULL;
    acache->pc = 0;

    return &acache->cache;
}
