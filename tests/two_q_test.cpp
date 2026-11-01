#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "caches/two_q.hpp"

namespace {

using Int2Q = caches::TwoQ<int, int>;
using Str2Q = caches::TwoQ<int, std::string>;

Int2Q MakeDefault() {
  return Int2Q(8);
}

// ---------- Constructor and capacity ----------

TEST(TwoQConstructor, ResolvesDefaultCapacities) {
  Int2Q cache(8);
  EXPECT_EQ(cache.capacity(), 8u);
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_TRUE(cache.empty());
}

TEST(TwoQConstructor, RespectsExplicitCapacities) {
  Int2Q cache(8, 3, 2);
  EXPECT_EQ(cache.capacity(), 8u);
  EXPECT_EQ(cache.size(), 0u);
}

TEST(TwoQConstructor, CapacityOneIsProblematic) {
  GTEST_SKIP() << "capacity=1 < in_capacity <=> capacity assertion <=> UB";
}

// ---------- Basic operations ----------

TEST(TwoQBasic, EmptyCacheMisses) {
  Int2Q cache = MakeDefault();
  EXPECT_EQ(cache.get(42), std::nullopt);
  EXPECT_EQ(cache.hits(), 0u);
  EXPECT_EQ(cache.misses(), 1u);
}

TEST(TwoQBasic, PutGoesToInAndGetHits) {
  Int2Q cache = MakeDefault();
  cache.put(1, 100);
  EXPECT_EQ(cache.size(), 1u);
  EXPECT_EQ(cache.get(1), 100);
  EXPECT_EQ(cache.hits(), 1u);
}

TEST(TwoQBasic, PutUpdatesExistingKey) {
  Int2Q cache = MakeDefault();
  cache.put(1, 100);
  cache.put(1, 111);
  EXPECT_EQ(cache.size(), 1u);
  EXPECT_EQ(cache.get(1), 111);
}

TEST(TwoQBasic, GetMissingKeyIsMiss) {
  Int2Q cache = MakeDefault();
  cache.put(1, 10);
  EXPECT_EQ(cache.get(999), std::nullopt);
  EXPECT_EQ(cache.size(), 1u);
  EXPECT_EQ(cache.misses(), 1u);
}

// ---------- In -> Hot promotion ----------

TEST(TwoQPromotion, SecondAccessPromotesToHot) {
  Int2Q cache(4, 1, 1);
  cache.put(1, 10);
  cache.put(2, 20);

  EXPECT_EQ(cache.get(2), 20);

  cache.put(3, 30);
  EXPECT_EQ(cache.get(3), 30);
  EXPECT_EQ(cache.get(2), 20);
}

TEST(TwoQPromotion, PutExistingInKeyPromotesToHot) {
  Int2Q cache(4, 1, 1);
  cache.put(1, 10);
  cache.put(1, 11);

  cache.put(2, 20);
  EXPECT_EQ(cache.get(1), 11);
  EXPECT_EQ(cache.get(2), 20);
}

// ---------- Ghost hits (in -> out -> hot) ----------

TEST(TwoQGhost, ReaccessEvictedInKeyPromotesToHot) {
  Int2Q cache(4, 1, 2);
  cache.put(1, 10);
  cache.put(2, 20);

  cache.put(1, 11);

  cache.put(3, 30);
  EXPECT_EQ(cache.get(1), 11);
  EXPECT_EQ(cache.get(3), 30);
  EXPECT_EQ(cache.get(2), std::nullopt);
}

TEST(TwoQGhost, GhostOnlyHoldsKeysNotPages) {
  Int2Q cache(4, 1, 2);
  cache.put(1, 10);
  cache.put(2, 20);
  EXPECT_EQ(cache.get(1), std::nullopt);
  EXPECT_EQ(cache.misses(), 1u);
}

TEST(TwoQGhost, OutOverflowEvictsOldestGhost) {
  Int2Q cache(4, 1, 1);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);
  cache.put(1, 11);
  EXPECT_EQ(cache.get(1), 11);
  EXPECT_EQ(cache.get(2), std::nullopt);
}

// ---------- Hot eviction (LRU within hot) ----------

TEST(TwoQHot, EvictsLeastRecentlyUsedInHot) {
  Int2Q cache(4, 1, 1);
  cache.put(1, 10);
  cache.get(1);
  cache.put(2, 20);
  cache.get(2);
  cache.put(3, 30);
  cache.get(3);

  cache.put(4, 40);
  cache.put(5, 50);

  EXPECT_EQ(cache.get(1), 10);

  cache.put(6, 60);

  EXPECT_EQ(cache.get(3), 30);
  EXPECT_EQ(cache.get(2), 20);
  EXPECT_EQ(cache.get(1), 10);
}

TEST(TwoQHot, HotCapacityRespected) {
  Int2Q cache(4, 1, 1);
  cache.put(1, 10);
  cache.get(1);
  cache.put(2, 20);
  cache.get(2);
  cache.put(3, 30);
  cache.get(3);
  cache.put(4, 40);
  cache.get(4);
  EXPECT_EQ(cache.get(1), std::nullopt);
  EXPECT_EQ(cache.get(2), 20);
  EXPECT_EQ(cache.get(3), 30);
  EXPECT_EQ(cache.get(4), 40);
}

// ---------- Invariants ----------

TEST(TwoQInvariants, SizeNeverExceedsCapacity) {
  Int2Q cache(8);
  for (int i = 0; i < 100; ++i) {
    cache.put(i, i * 10);
    EXPECT_LE(cache.size(), cache.capacity());
  }
}

TEST(TwoQInvariants, ResidentAndGhostsDisjoint) {
  Int2Q cache(4, 1, 2);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(1, 11);
  EXPECT_EQ(cache.size(), 2u);
  EXPECT_EQ(cache.get(1), 11);
}

// ---------- Statistics ----------

TEST(TwoQStats, HitsAndMissesCounted) {
  Int2Q cache = MakeDefault();
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

TEST(TwoQStats, ClearResetsStatsAndContent) {
  Int2Q cache = MakeDefault();
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

// ---------- Move semantics ----------

TEST(TwoQMove, MoveConstructTransfersState) {
  Int2Q src = MakeDefault();
  src.put(1, 10);
  src.put(2, 20);
  src.get(1);

  Int2Q dst(std::move(src));

  EXPECT_EQ(dst.capacity(), 8u);
  EXPECT_EQ(dst.size(), 2u);
  EXPECT_EQ(dst.get(1), 10);
  EXPECT_EQ(dst.get(2), 20);
}

TEST(TwoQMove, MoveAssignTransfersState) {
  Int2Q src(4, 1, 1);
  src.put(7, 70);

  Int2Q dst(8);
  dst.put(1, 10);

  dst = std::move(src);

  EXPECT_EQ(dst.capacity(), 4u);
  EXPECT_EQ(dst.size(), 1u);
  EXPECT_EQ(dst.get(7), 70);
}

TEST(TwoQMove, MovedFromCacheRemainsUsable) {
  Int2Q src(4, 1, 1);
  src.put(1, 10);
  Int2Q dst(std::move(src));

  EXPECT_EQ(src.size(), 0u);
  src.put(5, 50);
  EXPECT_EQ(src.get(5), 50);
  EXPECT_EQ(dst.get(1), 10);
}

// ---------- Polymorphism through ICache ----------

TEST(TwoQPolymorphic, WorksThroughBasePointer) {
  std::unique_ptr<caches::ICache<int, int>> cache = std::make_unique<Int2Q>(8);
  cache->put(1, 10);
  cache->put(2, 20);
  EXPECT_EQ(cache->get(1), 10);
  EXPECT_EQ(cache->size(), 2u);
  EXPECT_EQ(cache->capacity(), 8u);
  EXPECT_EQ(cache->hits(), 1u);
  EXPECT_EQ(cache->misses(), 0u);
}

// ---------- Non-trivial types ----------

TEST(TwoQTypes, WorksWithStringPage) {
  Str2Q cache(8, 4, 2);
  cache.put(1, "one");
  cache.put(2, "two");
  EXPECT_EQ(cache.get(1), std::string("one"));
  cache.put(3, "three");
  EXPECT_EQ(cache.get(3), std::string("three"));
}

// ---------- Compile-time invariants ----------

TEST(TwoQCompileTime, SpecialMembers) {
  static_assert(!std::is_copy_constructible_v<Int2Q>);
  static_assert(!std::is_copy_assignable_v<Int2Q>);
  static_assert(std::is_move_constructible_v<Int2Q>);
  static_assert(std::is_move_assignable_v<Int2Q>);
  static_assert(std::has_virtual_destructor_v<caches::ICache<int, int>>);
}

}  // namespace
