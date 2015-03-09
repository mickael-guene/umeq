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
#include <stdio.h>
#include <stdint.h>
#include "gtest/gtest.h"
#include "cache.h"

TEST(Cache, createRemove) {
    char memory[MIN_CACHE_SIZE];
    struct cache *cache;

    cache = createCache(memory, MIN_CACHE_SIZE, 0);
    removeCache(cache);
}

TEST(Cache, lookupFailed) {
    char memory[MIN_CACHE_SIZE];
    struct cache *cache;
    void *cache_hit;

    cache = createCache(memory, MIN_CACHE_SIZE, 0);
    
    cache_hit = cache->lookup(cache, 0x8000);
    EXPECT_TRUE(cache_hit == NULL);

    removeCache(cache);
}

TEST(Cache, lookupSuccess) {
    char memory[MIN_CACHE_SIZE];
    char data[16];
    struct cache *cache;
    char *cache_hit;
    int i;

    for(i = 0; i < sizeof(data); i++) {
        data[i] = i;
    }
    cache = createCache(memory, MIN_CACHE_SIZE, 0);
    
    cache->append(cache, 0x8000, data, sizeof(data));
    cache_hit = (char *) cache->lookup(cache, 0x8000);
    EXPECT_TRUE(cache_hit != NULL);
    for(i = 0; i < sizeof(data); i++) {
        EXPECT_EQ(cache_hit[i], data[i]);
    }

    removeCache(cache);
}

TEST(Cache, multipleCache) {
    /* FIXME: keep like this of increase stacksize limit */
    static char memory[2][MIN_CACHE_SIZE];
    struct cache *cache[2];
    char *cache_hit;
    char data[16];
    int i;

    for(i = 0; i < sizeof(data); i++) {
        data[i] = i;
    }
    for(i = 0; i < 2; i++) {
        cache[i] = createCache(memory[i], MIN_CACHE_SIZE, 0);
    }
    cache[0]->append(cache[0], 0x8000, data, sizeof(data));
    cache[1]->append(cache[1], 0x18000, data, sizeof(data));
    /* be sure to have hit for cache[0] and miss for cache[1] for 0x8000 */
    cache_hit = (char *) cache[0]->lookup(cache[0], 0x8000);
    EXPECT_TRUE(cache_hit != NULL);
    for(i = 0; i < sizeof(data); i++) {
        EXPECT_EQ(cache_hit[i], data[i]);
    }
    cache_hit = (char *) cache[1]->lookup(cache[1], 0x8000);
    EXPECT_TRUE(cache_hit == NULL);
    /* be sure to have hit for cache[1] and miss for cache[0] for 0x8000 */
    cache_hit = (char *) cache[0]->lookup(cache[0], 0x18000);
    EXPECT_TRUE(cache_hit == NULL);
    cache_hit = (char *) cache[1]->lookup(cache[1], 0x18000);
    EXPECT_TRUE(cache_hit != NULL);
    for(i = 0; i < sizeof(data); i++) {
        EXPECT_EQ(cache_hit[i], data[i]);
    }

    for(i = 0; i < 2; i++) {
        removeCache(cache[i]);
    }
}

TEST(Cache, cleanCache) {
    char memory[MIN_CACHE_SIZE];
    char data[16];
    struct cache *cache;
    char *cache_hit;
    int i;

    for(i = 0; i < sizeof(data); i++) {
        data[i] = i;
    }
    cache = createCache(memory, MIN_CACHE_SIZE, 0);
    
    cache->append(cache, 0x8000, data, sizeof(data));
    cache_hit = (char *) cache->lookup(cache, 0x8000);
    EXPECT_TRUE(cache_hit != NULL);
    for(i = 0; i < sizeof(data); i++) {
        EXPECT_EQ(cache_hit[i], data[i]);
    }
    cleanCaches(0, ~0);
    cache_hit = (char *) cache->lookup(cache, 0x8000);
    EXPECT_TRUE(cache_hit == NULL);

    removeCache(cache);
}

TEST(Cache, cleanCacheMultiple) {
    /* FIXME: keep like this of increase stacksize limit */
    static char memory[2][MIN_CACHE_SIZE];
    struct cache *cache[2];
    char *cache_hit;
    char data[16];
    int i;

    for(i = 0; i < sizeof(data); i++) {
        data[i] = i;
    }
    for(i = 0; i < 2; i++) {
        cache[i] = createCache(memory[i], MIN_CACHE_SIZE, 0);
    }
    cache[0]->append(cache[0], 0x8000, data, sizeof(data));
    cache[1]->append(cache[1], 0x18000, data, sizeof(data));
    /* be sure to have hit for cache[0] for 0x8000 and hit for cache[1] for 0x18000 */
    cache_hit = (char *) cache[0]->lookup(cache[0], 0x8000);
    EXPECT_TRUE(cache_hit != NULL);
    for(i = 0; i < sizeof(data); i++) {
        EXPECT_EQ(cache_hit[i], data[i]);
    }
    cache_hit = (char *) cache[1]->lookup(cache[1], 0x18000);
    EXPECT_TRUE(cache_hit != NULL);
    for(i = 0; i < sizeof(data); i++) {
        EXPECT_EQ(cache_hit[i], data[i]);
    }
    /* be sure to have cache miss after cleanCaches */
    cleanCaches(0, ~0);
    cache_hit = (char *) cache[0]->lookup(cache[0], 0x8000);
    EXPECT_TRUE(cache_hit == NULL);
    cache_hit = (char *) cache[1]->lookup(cache[1], 0x18000);
    EXPECT_TRUE(cache_hit == NULL);

    for(i = 0; i < 2; i++) {
        removeCache(cache[i]);
    }
}
