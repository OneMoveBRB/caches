#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "caches/lfu.hpp"

namespace {

using IntLFU = caches::LFU<int, int>;
using StrLFU = caches::LFU<int, std::string>;

// ---------- Basic operations ----------

TEST(LFUBasic, EmptyCacheMisses) {
  IntLFU cache(3);
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_EQ(cache.capacity(), 3u);
  EXPECT_TRUE(cache.empty());
  EXPECT_FALSE(cache.full());
  EXPECT_EQ(cache.get(42), std::nullopt);
  EXPECT_EQ(cache.hits(), 0u);
  EXPECT_EQ(cache.misses(), 1u);
}

TEST(LFUBasic, PutThenGetHits) {
  IntLFU cache(2);
  cache.put(1, 100);
  cache.put(2, 200);

  EXPECT_EQ(cache.get(1), 100);
  EXPECT_EQ(cache.get(2), 200);
  EXPECT_EQ(cache.size(), 2u);
  EXPECT_TRUE(cache.full());
  EXPECT_EQ(cache.hits(), 2u);
  EXPECT_EQ(cache.misses(), 0u);
}

TEST(LFUBasic, PutUpdatesExistingKey) {
  IntLFU cache(2);
  cache.put(1, 100);
  cache.put(1, 111);

  EXPECT_EQ(cache.size(), 1u);
  EXPECT_EQ(cache.get(1), 111);
}

TEST(LFUBasic, GetMissingKeyIsMissAndDoesNotChangeSize) {
  IntLFU cache(2);
  cache.put(1, 10);
  EXPECT_EQ(cache.get(999), std::nullopt);
  EXPECT_EQ(cache.size(), 1u);
  EXPECT_EQ(cache.misses(), 1u);
  EXPECT_EQ(cache.hits(), 0u);
}

// ---------- LFU eviction ----------

TEST(LFUEviction, EvictsLeastFrequentlyUsed) {
  IntLFU cache(2);
  cache.put(1, 10);
  cache.put(2, 20);

  cache.get(1);
  cache.get(1);

  cache.put(3, 30);

  EXPECT_EQ(cache.get(1), 10);
  EXPECT_EQ(cache.get(3), 30);
  EXPECT_EQ(cache.get(2), std::nullopt);
}

TEST(LFUEviction, EvictsInFifoOrderOnFrequencyTie) {
  IntLFU cache(3);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);

  cache.put(4, 40);
  cache.put(5, 50);

  EXPECT_EQ(cache.get(1), std::nullopt);
  EXPECT_EQ(cache.get(2), std::nullopt);
  EXPECT_EQ(cache.get(3), 30);
  EXPECT_EQ(cache.get(4), 40);
  EXPECT_EQ(cache.get(5), 50);
}

TEST(LFUEviction, GetRefreshesFrequencySoItIsNotEvicted) {
  IntLFU cache(2);
  cache.put(1, 10);
  cache.put(2, 20);

  cache.get(2);

  cache.put(3, 30);

  EXPECT_EQ(cache.get(1), std::nullopt);
  EXPECT_EQ(cache.get(2), 20);
  EXPECT_EQ(cache.get(3), 30);
}

TEST(LFUEviction, PutExistingKeyRefreshesFrequency) {
  IntLFU cache(2);
  cache.put(1, 10);
  cache.put(2, 20);

  cache.put(1, 11);

  cache.put(3, 30);

  EXPECT_EQ(cache.get(1), 11);
  EXPECT_EQ(cache.get(2), std::nullopt);
  EXPECT_EQ(cache.get(3), 30);
}

TEST(LFUEviction, CapacityOne) {
  IntLFU cache(1);
  cache.put(1, 10);
  cache.put(2, 20);
  EXPECT_EQ(cache.get(1), std::nullopt);
  EXPECT_EQ(cache.get(2), 20);
  EXPECT_EQ(cache.size(), 1u);
}

// ---------- min_freq_ and bucket transitions ----------

TEST(LFUFreq, MinFreqAdvancesAfterEviction) {
  IntLFU cache(3);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);

  cache.get(1);
  cache.get(1);
  cache.get(2);

  cache.put(4, 40);
  cache.put(5, 50);

  EXPECT_EQ(cache.get(3), std::nullopt);
  EXPECT_EQ(cache.get(4), std::nullopt);
  EXPECT_EQ(cache.get(1), 10);
  EXPECT_EQ(cache.get(2), 20);
  EXPECT_EQ(cache.get(5), 50);
}

TEST(LFUFreq, MinFreqMovesToSmallerBucketWhenItEmpties) {
  IntLFU cache(2);
  cache.put(1, 10);
  cache.put(2, 20);

  cache.get(1);
  cache.get(1);

  cache.put(3, 30);

  cache.put(4, 40);
  EXPECT_EQ(cache.get(1), 10);
  EXPECT_EQ(cache.get(3), std::nullopt);
}

// ---------- Statistics ----------

TEST(LFUStats, HitsAndMissesCounted) {
  IntLFU cache(2);
  cache.get(1);
  cache.put(1, 10);
  cache.get(1);
  cache.get(2);
  cache.put(2, 20);
  cache.get(2);
  cache.get(2);

  EXPECT_EQ(cache.hits(), 3u);
  EXPECT_EQ(cache.misses(), 2u);
}

TEST(LFUStats, ClearResetsStatsAndContent) {
  IntLFU cache(2);
  cache.put(1, 10);
  cache.get(1);
  cache.get(2);

  cache.clear();

  EXPECT_EQ(cache.size(), 0u);
  EXPECT_TRUE(cache.empty());
  EXPECT_EQ(cache.hits(), 0u);
  EXPECT_EQ(cache.misses(), 0u);
  EXPECT_EQ(cache.get(1), std::nullopt);
}

// ---------- Capacity ----------

TEST(LFUCapacity, ZeroCapacityNeverStores) {
  IntLFU cache(0);
  cache.put(1, 10);
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_TRUE(cache.empty());
  EXPECT_TRUE(cache.full());
  EXPECT_EQ(cache.get(1), std::nullopt);
}

// ---------- Move semantics ----------

TEST(LFUMove, MoveConstructTransfersState) {
  IntLFU src(2);
  src.put(1, 10);
  src.put(2, 20);
  src.get(1);

  IntLFU dst(std::move(src));

  EXPECT_EQ(dst.capacity(), 2u);
  EXPECT_EQ(dst.size(), 2u);
  EXPECT_EQ(dst.get(1), 10);
  EXPECT_EQ(dst.get(2), 20);
}

TEST(LFUMove, MoveAssignTransfersState) {
  IntLFU src(3);
  src.put(7, 70);

  IntLFU dst(1);
  dst.put(1, 10);

  dst = std::move(src);

  EXPECT_EQ(dst.capacity(), 3u);
  EXPECT_EQ(dst.size(), 1u);
  EXPECT_EQ(dst.get(7), 70);
}

TEST(LFUMove, MovedFromCacheRemainsUsable) {
  IntLFU src(2);
  src.put(1, 10);

  IntLFU dst(std::move(src));

  EXPECT_EQ(src.size(), 0u);
  src.put(5, 50);
  EXPECT_EQ(src.get(5), 50);
  EXPECT_EQ(dst.get(1), 10);
}

// ---------- Polymorphism through ICache ----------

TEST(LFUPolymorphic, WorksThroughBasePointer) {
  std::unique_ptr<caches::ICache<int, int>> cache = std::make_unique<IntLFU>(2);

  cache->put(1, 10);
  cache->put(2, 20);
  EXPECT_EQ(cache->get(1), 10);
  EXPECT_EQ(cache->size(), 2u);
  EXPECT_EQ(cache->capacity(), 2u);
  EXPECT_EQ(cache->hits(), 1u);
  EXPECT_EQ(cache->misses(), 0u);
}

// ---------- Non-trivial types ----------

TEST(LFUTypes, WorksWithStringPage) {
  StrLFU cache(2);
  cache.put(1, "one");
  cache.put(2, "two");
  EXPECT_EQ(cache.get(1), std::string("one"));
  cache.put(3, "three");
  EXPECT_EQ(cache.get(2), std::nullopt);
  EXPECT_EQ(cache.get(3), std::string("three"));
}

// ---------- Compile-time invariants ----------

TEST(LFUCompileTime, SpecialMembers) {
  static_assert(!std::is_copy_constructible_v<IntLFU>);
  static_assert(!std::is_copy_assignable_v<IntLFU>);
  static_assert(std::is_move_constructible_v<IntLFU>);
  static_assert(std::is_move_assignable_v<IntLFU>);
  static_assert(std::has_virtual_destructor_v<caches::ICache<int, int>>);
}

}  // namespace
