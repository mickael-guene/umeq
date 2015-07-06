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

#include <stdlib.h>
#include "jitter_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __CACHE__
#define __CACHE__ 1

#define MIN_CACHE_SIZE          (1 * 1024 * 1024)
#define MIN_CACHE_SIZE_NONE     (4 * 1024)

struct cache {
    void *(*lookup)(struct cache *cache, uint64_t pc, int *cache_clean_event);
    void *(*append)(struct cache *cache, uint64_t pc, void *data, int size, int *cache_clean_event);
    uint64_t (*lookup_pc)(struct cache *cache, void *host_pc, void **host_pc_start);
};

struct cache *createCache(void *memory, int size, int nb_of_pc_bit_to_drop);
void removeCache(struct cache *cache);
void cleanCaches(uint64_t from_pc, uint64_t to_pc_exclude);

struct cache *createCacheNone(void *memory, int size);

#endif

#ifdef __cplusplus
}
#endif

