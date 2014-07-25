#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "gtest/gtest.h"
#include "cache.h"

TEST(Cache, createRemove) {
    char memory[CACHE_SIZE];
    struct cache *cache;

    cache = createArmCache(memory);
    removeArmCache(cache);
}

TEST(Cache, lookupFailed) {
    char memory[CACHE_SIZE];
    struct cache *cache;
    void *cache_hit;

    cache = createArmCache(memory);
    
    cache_hit = cache->lookup(cache, 0x8000);
    EXPECT_TRUE(cache_hit == NULL);

    removeArmCache(cache);
}

TEST(Cache, lookupSuccess) {
    char memory[CACHE_SIZE];
    char data[16];
    struct cache *cache;
    char *cache_hit;
    int i;

    for(i = 0; i < sizeof(data); i++) {
        data[i] = i;
    }
    cache = createArmCache(memory);
    
    cache->append(cache, 0x8000, data, sizeof(data));
    cache_hit = (char *) cache->lookup(cache, 0x8000);
    EXPECT_TRUE(cache_hit != NULL);
    for(i = 0; i < sizeof(data); i++) {
        EXPECT_EQ(cache_hit[i], data[i]);
    }

    removeArmCache(cache);
}

TEST(Cache, multipleCache) {
    /* FIXME: keep like this of increase stacksize limit */
    static char memory[2][CACHE_SIZE];
    struct cache *cache[2];
    char *cache_hit;
    char data[16];
    int i;

    for(i = 0; i < sizeof(data); i++) {
        data[i] = i;
    }
    for(i = 0; i < 2; i++) {
        cache[i] = createArmCache(memory[i]);
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
        removeArmCache(cache[i]);
    }
}

TEST(Cache, cleanCache) {
    char memory[CACHE_SIZE];
    char data[16];
    struct cache *cache;
    char *cache_hit;
    int i;

    for(i = 0; i < sizeof(data); i++) {
        data[i] = i;
    }
    cache = createArmCache(memory);
    
    cache->append(cache, 0x8000, data, sizeof(data));
    cache_hit = (char *) cache->lookup(cache, 0x8000);
    EXPECT_TRUE(cache_hit != NULL);
    for(i = 0; i < sizeof(data); i++) {
        EXPECT_EQ(cache_hit[i], data[i]);
    }
    cleanCaches(0, ~0);
    cache_hit = (char *) cache->lookup(cache, 0x8000);
    EXPECT_TRUE(cache_hit == NULL);

    removeArmCache(cache);
}

TEST(Cache, cleanCacheMultiple) {
    /* FIXME: keep like this of increase stacksize limit */
    static char memory[2][CACHE_SIZE];
    struct cache *cache[2];
    char *cache_hit;
    char data[16];
    int i;

    for(i = 0; i < sizeof(data); i++) {
        data[i] = i;
    }
    for(i = 0; i < 2; i++) {
        cache[i] = createArmCache(memory[i]);
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
        removeArmCache(cache[i]);
    }
}
