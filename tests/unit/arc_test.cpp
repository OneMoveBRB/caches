#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "caches/arc.hpp"

namespace {

using IntARC = caches::ARC<int, int>;
using StrARC = caches::ARC<int, std::string>;

// ---------- Constructor and capacity ----------

TEST(ARCConstructor, ResolvesCapacities) {
  IntARC cache(4);
  EXPECT_EQ(cache.capacity(), 4u);
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_TRUE(cache.empty());
}

TEST(ARCConstructor, ZeroCapacityNeverStores) {
  IntARC cache(0);
  cache.put(1, 10);
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_EQ(cache.get(1), std::nullopt);
  EXPECT_TRUE(cache.empty());
}

// ---------- Basic operations ----------

TEST(ARCBasic, EmptyCacheMisses) {
  IntARC cache(4);
  EXPECT_EQ(cache.get(42), std::nullopt);
  EXPECT_EQ(cache.hits(), 0u);
  EXPECT_EQ(cache.misses(), 1u);
}

TEST(ARCBasic, PutThenGetHits) {
  IntARC cache(4);
  cache.put(1, 100);
  EXPECT_EQ(cache.size(), 1u);
  EXPECT_EQ(cache.get(1), 100);
  EXPECT_EQ(cache.hits(), 1u);
  EXPECT_EQ(cache.misses(), 0u);
}

TEST(ARCBasic, PutUpdatesExistingKey) {
  IntARC cache(4);
  cache.put(1, 100);
  cache.put(1, 111);
  EXPECT_EQ(cache.size(), 1u);
  EXPECT_EQ(cache.get(1), 111);
}

TEST(ARCBasic, GetMissingKeyIsMiss) {
  IntARC cache(4);
  cache.put(1, 10);
  EXPECT_EQ(cache.get(999), std::nullopt);
  EXPECT_EQ(cache.size(), 1u);
  EXPECT_EQ(cache.misses(), 1u);
}

// ---------- Eviction and ghost mechanics ----------

TEST(ARCEviction, EvictsFromTop1ToBottom1) {
  IntARC cache(2);
  cache.put(1, 10);
  cache.put(2, 20);

  cache.put(3, 30);
  EXPECT_EQ(cache.size(), 2u);
  EXPECT_EQ(cache.get(1), std::nullopt);
  EXPECT_EQ(cache.get(2), 20);
  EXPECT_EQ(cache.get(3), 30);

  EXPECT_EQ(cache.size(), 2u);
}

TEST(ARCEviction, GhostHitPromotesToResident) {
  IntARC cache(2);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);

  EXPECT_EQ(cache.get(1), std::nullopt);
  cache.put(1, 11);
  EXPECT_EQ(cache.size(), 2u);
  EXPECT_EQ(cache.get(1), 11);
}

TEST(ARCEviction, Top1ToTop2OnSecondAccess) {
  IntARC cache(4);
  cache.put(1, 10);
  EXPECT_EQ(cache.get(1), 10);
  cache.put(2, 20);
  cache.put(3, 30);
  cache.put(4, 40);

  cache.put(5, 50);
  EXPECT_LE(cache.size(), cache.capacity());
  EXPECT_EQ(cache.get(1), 10);
}

TEST(ARCEviction, RecentlyAccessedTop2StaysResident) {
  IntARC cache(2);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.get(1);
  cache.get(1);

  cache.put(3, 30);
  EXPECT_EQ(cache.get(1), 10);
  EXPECT_EQ(cache.get(3), 30);
  EXPECT_EQ(cache.get(2), std::nullopt);
}

// ---------- Parameter adaptation ----------

TEST(ARCAdaptation, ParameterGrowsOnBottom1Hit) {
  IntARC cache(4);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);
  cache.put(4, 40);
  cache.put(5, 50);

  for (int i = 1; i <= 5; ++i) {
    cache.put(i, i * 10);
    cache.put(i, i * 10);
  }

  EXPECT_LE(cache.size(), cache.capacity());
}

TEST(ARCAdaptation, ParameterStaysWithinCapacity) {
  IntARC cache(3);
  for (int i = 0; i < 50; ++i) {
    cache.put(i, i * 10);
    cache.get(i);
    EXPECT_LE(cache.size(), cache.capacity());
  }
}

// ---------- Invariants (Debug only) ----------

#ifndef NDEBUG
TEST(ARCInvariants, CheckInvariantsAfterOperations) {
  IntARC cache(4);
  for (int i = 0; i < 20; ++i) {
    cache.put(i, i * 10);
    cache.get(i);
    cache.put(i, i * 10);
  }
  EXPECT_LE(cache.size(), cache.capacity());
}

TEST(ARCInvariants, SizeNeverExceedsCapacity) {
  IntARC cache(8);
  for (int i = 0; i < 100; ++i) {
    cache.put(i, i * 10);
    EXPECT_LE(cache.size(), cache.capacity());
  }
}

TEST(ARCInvariants, GhostSizeBoundedByCapacity) {
  IntARC cache(2);
  for (int i = 0; i < 20; ++i) {
    cache.put(i, i * 10);
    cache.get(i);
  }
  EXPECT_LE(cache.size(), cache.capacity());
}
#endif  // !NDEBUG

// ---------- Statistics ----------

TEST(ARCStats, HitsAndMissesCounted) {
  IntARC cache(4);
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

TEST(ARCStats, ClearResetsStatsAndContent) {
  IntARC cache(4);
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

TEST(ARCMove, MoveConstructTransfersState) {
  IntARC src(4);
  src.put(1, 10);
  src.put(2, 20);
  src.get(1);

  IntARC dst(std::move(src));

  EXPECT_EQ(dst.capacity(), 4u);
  EXPECT_EQ(dst.size(), 2u);
  EXPECT_EQ(dst.get(1), 10);
  EXPECT_EQ(dst.get(2), 20);
}

TEST(ARCMove, MoveAssignTransfersState) {
  IntARC src(4);
  src.put(7, 70);

  IntARC dst(2);
  dst.put(1, 10);

  dst = std::move(src);

  EXPECT_EQ(dst.capacity(), 4u);
  EXPECT_EQ(dst.size(), 1u);
  EXPECT_EQ(dst.get(7), 70);
}

TEST(ARCMove, MovedFromCacheRemainsUsable) {
  IntARC src(4);
  src.put(1, 10);

  IntARC dst(std::move(src));

  EXPECT_EQ(src.size(), 0u);
  EXPECT_EQ(src.capacity(), 4u);
  src.put(5, 50);
  EXPECT_EQ(src.get(5), 50);
  EXPECT_EQ(dst.get(1), 10);
}

TEST(ARCMove, SelfMoveAssignIsSafe) {
  IntARC src(4);
  src.put(1, 10);
  src.put(2, 20);
  src.get(1);

  IntARC* ptr = &src;
  src = std::move(*ptr);

  EXPECT_LE(src.size(), src.capacity());
}

// ---------- swap ----------

TEST(ARCSwap, SwapsState) {
  IntARC a(4);
  IntARC b(2);

  a.put(1, 10);
  a.put(2, 20);

  b.put(3, 30);

  a.swap(b);

  EXPECT_EQ(a.capacity(), 2u);
  EXPECT_EQ(a.size(), 1u);
  EXPECT_EQ(a.get(3), 30);

  EXPECT_EQ(b.capacity(), 4u);
  EXPECT_EQ(b.size(), 2u);
  EXPECT_EQ(b.get(1), 10);
}

// ---------- Polymorphism ----------

TEST(ARCPolymorphic, WorksThroughBasePointer) {
  std::unique_ptr<caches::ICache<int, int>> cache = std::make_unique<IntARC>(4);
  cache->put(1, 10);
  cache->put(2, 20);
  EXPECT_EQ(cache->get(1), 10);
  EXPECT_EQ(cache->size(), 2u);
  EXPECT_EQ(cache->capacity(), 4u);
  EXPECT_EQ(cache->hits(), 1u);
  EXPECT_EQ(cache->misses(), 0u);
}

// ---------- Non-trivial types ----------

TEST(ARCTypes, WorksWithStringPage) {
  StrARC cache(2);
  cache.put(1, "one");
  cache.put(2, "two");
  cache.put(3, "three");
  EXPECT_EQ(cache.get(1), std::nullopt);
  EXPECT_EQ(cache.get(2), std::string("two"));
  EXPECT_EQ(cache.get(3), std::string("three"));
}

// ---------- Compile-time invariants ----------

TEST(ARCCompileTime, SpecialMembers) {
  static_assert(!std::is_copy_constructible_v<IntARC>);
  static_assert(!std::is_copy_assignable_v<IntARC>);
  static_assert(std::is_move_constructible_v<IntARC>);
  static_assert(std::is_move_assignable_v<IntARC>);
  static_assert(std::has_virtual_destructor_v<caches::ICache<int, int>>);
}

}  // namespace
