#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "caches/lru.hpp"

namespace {

using IntLRU = caches::LRU<int, int>;
using StrLRU = caches::LRU<int, std::string>;

// ---------- Basic operations ----------

TEST(LRUBasic, EmptyCacheMisses) {
  IntLRU cache(3);
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_EQ(cache.capacity(), 3u);
  EXPECT_EQ(cache.get(42), std::nullopt);
  EXPECT_EQ(cache.hits(), 0u);
  EXPECT_EQ(cache.misses(), 1u);
}

TEST(LRUBasic, PutThenGetHits) {
  IntLRU cache(2);
  cache.put(1, 100);
  cache.put(2, 200);

  EXPECT_EQ(cache.get(1), 100);
  EXPECT_EQ(cache.get(2), 200);
  EXPECT_EQ(cache.size(), 2u);
  EXPECT_EQ(cache.hits(), 2u);
  EXPECT_EQ(cache.misses(), 0u);
}

TEST(LRUBasic, PutUpdatesExistingKey) {
  IntLRU cache(2);
  cache.put(1, 100);
  cache.put(1, 111);

  EXPECT_EQ(cache.size(), 1u);
  EXPECT_EQ(cache.get(1), 111);
}

TEST(LRUBasic, GetMissingKeyIsMissAndDoesNotChangeSize) {
  IntLRU cache(2);
  cache.put(1, 10);
  EXPECT_EQ(cache.get(999), std::nullopt);
  EXPECT_EQ(cache.size(), 1u);
  EXPECT_EQ(cache.misses(), 1u);
  EXPECT_EQ(cache.hits(), 0u);
}

// ---------- LRU eviction ----------

TEST(LRUEviction, EvictsLeastRecentlyUsed) {
  IntLRU cache(2);
  cache.put(1, 10);
  cache.put(2, 20);

  // Touch 1 -> now 2 is the least recently used.
  EXPECT_EQ(cache.get(1), 10);

  cache.put(3, 30);  // should evict 2

  EXPECT_EQ(cache.size(), 2u);
  EXPECT_EQ(cache.get(1), 10);
  EXPECT_EQ(cache.get(3), 30);
  EXPECT_EQ(cache.get(2), std::nullopt);  // evicted
}

TEST(LRUEviction, EvictsInInsertionOrderWithoutAccess) {
  IntLRU cache(3);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);

  cache.put(4, 40);  // evicts 1
  cache.put(5, 50);  // evicts 2

  EXPECT_EQ(cache.get(1), std::nullopt);
  EXPECT_EQ(cache.get(2), std::nullopt);
  EXPECT_EQ(cache.get(3), 30);
  EXPECT_EQ(cache.get(4), 40);
  EXPECT_EQ(cache.get(5), 50);
}

TEST(LRUEviction, PutExistingKeyRefreshesRecency) {
  IntLRU cache(2);
  cache.put(1, 10);
  cache.put(2, 20);

  cache.put(1, 11);  // update -> 1 becomes the most recent

  cache.put(3, 30);  // should evict 2, not 1

  EXPECT_EQ(cache.get(1), 11);
  EXPECT_EQ(cache.get(2), std::nullopt);
  EXPECT_EQ(cache.get(3), 30);
}

TEST(LRUEviction, CapacityOne) {
  IntLRU cache(1);
  cache.put(1, 10);
  cache.put(2, 20);
  EXPECT_EQ(cache.get(1), std::nullopt);
  EXPECT_EQ(cache.get(2), 20);
  EXPECT_EQ(cache.size(), 1u);
}

// ---------- Statistics ----------

TEST(LRUStats, HitsAndMissesCounted) {
  IntLRU cache(2);
  cache.get(1);            // miss
  cache.put(1, 10);
  cache.get(1);            // hit
  cache.get(2);            // miss
  cache.put(2, 20);
  cache.get(2);            // hit
  cache.get(2);            // hit

  EXPECT_EQ(cache.hits(), 3u);
  EXPECT_EQ(cache.misses(), 2u);
}

TEST(LRUStats, ClearResetsStatsAndContent) {
  IntLRU cache(2);
  cache.put(1, 10);
  cache.get(1);
  cache.get(2);

  cache.clear();

  EXPECT_EQ(cache.size(), 0u);
  EXPECT_EQ(cache.hits(), 0u);
  EXPECT_EQ(cache.misses(), 0u);
  EXPECT_EQ(cache.get(1), std::nullopt);  // empty -> miss
}

// ---------- Capacity ----------

TEST(LRUCapacity, ZeroCapacityNeverStores) {
  IntLRU cache(0);
  cache.put(1, 10);
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_EQ(cache.get(1), std::nullopt);
  // still usable, still stores nothing
  cache.put(2, 20);
  EXPECT_EQ(cache.size(), 0u);
}

// ---------- Move semantics ----------

TEST(LRUMove, MoveConstructTransfersState) {
  IntLRU src(2);
  src.put(1, 10);
  src.put(2, 20);
  src.get(1);

  IntLRU dst(std::move(src));

  EXPECT_EQ(dst.capacity(), 2u);
  EXPECT_EQ(dst.size(), 2u);
  EXPECT_EQ(dst.get(1), 10);
  EXPECT_EQ(dst.get(2), 20);
}

TEST(LRUMove, MoveAssignTransfersState) {
  IntLRU src(3);
  src.put(7, 70);

  IntLRU dst(1);
  dst.put(1, 10);

  dst = std::move(src);

  EXPECT_EQ(dst.capacity(), 3u);
  EXPECT_EQ(dst.size(), 1u);
  EXPECT_EQ(dst.get(7), 70);
}

TEST(LRUMove, MovedFromCacheRemainsUsable) {
  IntLRU src(2);
  src.put(1, 10);

  IntLRU dst(std::move(src));

  // Source must remain in a valid (empty) state.
  EXPECT_EQ(src.size(), 0u);
  src.put(5, 50);
  EXPECT_EQ(src.get(5), 50);
  EXPECT_EQ(dst.get(1), 10);  // destination is not affected
}

// ---------- Polymorphism through ICache ----------

TEST(LRUPolymorphic, WorksThroughBasePointer) {
  std::unique_ptr<caches::ICache<int, int>> cache = std::make_unique<IntLRU>(2);

  cache->put(1, 10);
  cache->put(2, 20);
  EXPECT_EQ(cache->get(1), 10);
  EXPECT_EQ(cache->size(), 2u);
  EXPECT_EQ(cache->capacity(), 2u);
  EXPECT_EQ(cache->hits(), 1u);
  EXPECT_EQ(cache->misses(), 0u);
}

// ---------- Non-trivial types ----------

TEST(LRUTypes, WorksWithStringPage) {
  StrLRU cache(2);
  cache.put(1, "one");
  cache.put(2, "two");
  EXPECT_EQ(cache.get(1), std::string("one"));
  cache.put(3, "three");  // evicts 2
  EXPECT_EQ(cache.get(2), std::nullopt);
  EXPECT_EQ(cache.get(3), std::string("three"));
}

TEST(LRUTypes, MoveOnlyPageType) {
  // PageT must be copyable for put(const PageT&); here we just verify the
  // class handles types that are expensive to copy correctly.
  caches::LRU<int, std::string> cache(1);
  std::string big(1024, 'x');
  cache.put(1, big);
  EXPECT_EQ(cache.get(1).value().size(), 1024u);
}

}  // namespace
