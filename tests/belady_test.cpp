#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <vector>

#include "caches/belady.hpp"

namespace {

using IntOpt = caches::OptimalCache<int>;
using Res = IntOpt::Result;
using AccessResult = IntOpt::AccessResult;

// ---------- Constructor and initial state ----------

TEST(BeladyBasic, EmptyState) {
  IntOpt cache(3, {1, 2, 3});
  EXPECT_EQ(cache.capacity(), 3u);
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_EQ(cache.processed_requests(), 0u);
  EXPECT_TRUE(cache.has_next_request());
  EXPECT_EQ(cache.stats().hits, 0u);
  EXPECT_EQ(cache.stats().misses, 0u);
  EXPECT_EQ(cache.stats().total(), 0u);
  EXPECT_DOUBLE_EQ(cache.stats().hit_rate(), 0.0);
  EXPECT_DOUBLE_EQ(cache.stats().miss_rate(), 0.0);
}

TEST(BeladyBasic, NoRequests) {
  IntOpt cache(3, {});
  EXPECT_FALSE(cache.has_next_request());
  EXPECT_EQ(cache.processed_requests(), 0u);

  auto results = cache.simulate();
  EXPECT_TRUE(results.empty());
  EXPECT_EQ(cache.stats().total(), 0u);
}

// ---------- Single request ----------

TEST(BeladyBasic, SingleMiss) {
  IntOpt cache(2, {42});
  AccessResult result = cache.access(42);
  EXPECT_EQ(result.result, Res::Miss);
  EXPECT_EQ(result.evicted, std::nullopt);
  EXPECT_EQ(cache.size(), 1u);
  EXPECT_TRUE(cache.contains(42));
  EXPECT_EQ(cache.stats().misses, 1u);
  EXPECT_EQ(cache.stats().hits, 0u);
  EXPECT_FALSE(cache.has_next_request());
}

TEST(BeladyBasic, SingleHitAfterPut) {
  IntOpt cache(2, {42, 42});
  cache.access(42);
  AccessResult result = cache.access(42);
  EXPECT_EQ(result.result, Res::Hit);
  EXPECT_EQ(result.evicted, std::nullopt);
  EXPECT_EQ(cache.stats().hits, 1u);
  EXPECT_EQ(cache.stats().misses, 1u);
  EXPECT_EQ(cache.size(), 1u);
}

// ---------- Optimal eviction ----------

TEST(BeladyEviction, EvictsFarthestNextUse) {
  std::vector<int> req = {1, 2, 3, 1, 2, 4, 1, 2, 3, 4};
  IntOpt cache(3, req);

  for (int key : {1, 2, 3}) {
    auto result = cache.access(key);
    EXPECT_EQ(result.result, Res::Miss);
    EXPECT_EQ(result.evicted, std::nullopt);
  }

  EXPECT_EQ(cache.access(1).result, Res::Hit);
  EXPECT_EQ(cache.access(2).result, Res::Hit);

  auto result = cache.access(4);
  EXPECT_EQ(result.result, Res::Miss);
  ASSERT_TRUE(result.evicted.has_value());
  EXPECT_EQ(*result.evicted, 3);
  EXPECT_FALSE(cache.contains(3));
  EXPECT_TRUE(cache.contains(1));
  EXPECT_TRUE(cache.contains(2));
  EXPECT_TRUE(cache.contains(4));
}

TEST(BeladyEviction, EvictsElementNotUsedAgain) {
  std::vector<int> req = {1, 2, 3};
  IntOpt cache(2, req);
  cache.access(1);
  cache.access(2);
  auto result = cache.access(3);
  EXPECT_EQ(result.result, Res::Miss);
  ASSERT_TRUE(result.evicted.has_value());
  EXPECT_EQ(*result.evicted, 2);
  EXPECT_TRUE(cache.contains(1));
  EXPECT_TRUE(cache.contains(3));
}

TEST(BeladyEviction, DoesNotEvictKeyUsedSoon) {
  std::vector<int> req = {1, 2, 1, 2};
  IntOpt cache(1, req);
  cache.access(1);
  auto result = cache.access(2);
  ASSERT_TRUE(result.evicted.has_value());
  EXPECT_EQ(*result.evicted, 1);
  EXPECT_FALSE(cache.contains(1));
  EXPECT_TRUE(cache.contains(2));
}

// ---------- Full simulation ----------

TEST(BeladySimulate, TextbookExample) {
  std::vector<int> req = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
  IntOpt cache(3, req);

  auto results = cache.simulate();
  ASSERT_EQ(results.size(), req.size());

  std::size_t hits = 0, misses = 0;
  for (const auto& result : results) {
    if (result.result == Res::Hit)
      ++hits;
    else
      ++misses;
  }
  EXPECT_EQ(cache.stats().hits, hits);
  EXPECT_EQ(cache.stats().misses, misses);
  EXPECT_EQ(cache.stats().total(), req.size());
}

TEST(BeladySimulate, SimulateResetsState) {
  IntOpt cache(2, {1, 2, 3, 1, 2});
  cache.simulate();
  EXPECT_FALSE(cache.has_next_request());

  auto r1 = cache.simulate();
  auto r2 = cache.simulate();
  ASSERT_EQ(r1.size(), r2.size());
  for (std::size_t idx = 0; idx < r1.size(); ++idx) {
    EXPECT_EQ(r1[idx].result, r2[idx].result);
    EXPECT_EQ(r1[idx].evicted, r2[idx].evicted);
  }
}

TEST(BeladySimulate, ResetClearsState) {
  IntOpt cache(2, {1, 2, 3});
  cache.access(1);
  cache.access(2);
  EXPECT_EQ(cache.size(), 2u);

  cache.reset();
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_EQ(cache.processed_requests(), 0u);
  EXPECT_EQ(cache.stats().hits, 0u);
  EXPECT_EQ(cache.stats().misses, 0u);
  EXPECT_TRUE(cache.has_next_request());
}

// ---------- Capacity ----------

TEST(BeladyCapacity, ZeroCapacityAlwaysMisses) {
  IntOpt cache(0, {1, 2, 1, 2, 1});
  auto results = cache.simulate();
  for (const auto& result : results) {
    EXPECT_EQ(result.result, Res::Miss);
    EXPECT_EQ(result.evicted, std::nullopt);
  }
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_EQ(cache.capacity(), 0u);
  EXPECT_EQ(cache.stats().misses, 5u);
  EXPECT_EQ(cache.stats().hits, 0u);
}

TEST(BeladyCapacity, CapacityOne) {
  IntOpt cache(1, {1, 2, 1, 2});
  auto results = cache.simulate();
  EXPECT_EQ(results[0].result, Res::Miss);
  EXPECT_EQ(results[1].result, Res::Miss);
  ASSERT_TRUE(results[1].evicted.has_value());
  EXPECT_EQ(*results[1].evicted, 1);
  EXPECT_EQ(results[2].result, Res::Miss);
  ASSERT_TRUE(results[2].evicted.has_value());
  EXPECT_EQ(*results[2].evicted, 2);
  EXPECT_EQ(results[3].result, Res::Miss);
  EXPECT_EQ(cache.stats().misses, 4u);
  EXPECT_EQ(cache.stats().hits, 0u);
}

TEST(BeladyCapacity, CapacityEqualsDistinctKeys) {
  IntOpt cache(3, {1, 2, 3, 1, 2, 3});
  auto results = cache.simulate();
  EXPECT_EQ(results[0].result, Res::Miss);
  EXPECT_EQ(results[1].result, Res::Miss);
  EXPECT_EQ(results[2].result, Res::Miss);
  EXPECT_EQ(results[3].result, Res::Hit);
  EXPECT_EQ(results[4].result, Res::Hit);
  EXPECT_EQ(results[5].result, Res::Hit);
  EXPECT_EQ(cache.stats().misses, 3u);
  EXPECT_EQ(cache.stats().hits, 3u);
}

TEST(BeladyCapacity, CapacityExceedsDistinctKeys) {
  IntOpt cache(10, {1, 2, 1, 2});
  auto results = cache.simulate();
  EXPECT_EQ(results[0].result, Res::Miss);
  EXPECT_EQ(results[1].result, Res::Miss);
  EXPECT_EQ(results[2].result, Res::Hit);
  EXPECT_EQ(results[3].result, Res::Hit);
  EXPECT_EQ(cache.size(), 2u);
}

// ---------- Key type ----------

TEST(BeladyTypes, StringKeys) {
  using StrOpt = caches::OptimalCache<std::string>;
  StrOpt cache(2, {"a", "b", "a", "c"});
  auto results = cache.simulate();
  EXPECT_EQ(results[0].result, StrOpt::Result::Miss);
  EXPECT_EQ(results[1].result, StrOpt::Result::Miss);
  EXPECT_EQ(results[2].result, StrOpt::Result::Hit);
  EXPECT_EQ(results[3].result, StrOpt::Result::Miss);
  ASSERT_TRUE(results[3].evicted.has_value());
  EXPECT_EQ(*results[3].evicted, std::string("b"));
}

// ---------- Statistics ----------

TEST(BeladyStats, HitRateAndMissRate) {
  IntOpt cache(2, {1, 2, 1, 1, 2});
  cache.simulate();
  EXPECT_EQ(cache.stats().hits, 3u);
  EXPECT_EQ(cache.stats().misses, 2u);
  EXPECT_EQ(cache.stats().total(), 5u);
  EXPECT_DOUBLE_EQ(cache.stats().hit_rate(), 3.0 / 5.0);
  EXPECT_DOUBLE_EQ(cache.stats().miss_rate(), 2.0 / 5.0);
}

// ---------- Debug-only checks ----------

#ifndef NDEBUG
TEST(BeladyDebug, AccessBeyondPlannedRequestsAsserts) {
  IntOpt cache(1, {1});
  cache.access(1);
  EXPECT_FALSE(cache.has_next_request());
  EXPECT_DEATH(cache.access(1), "No more planned requests");
}
#endif  // !NDEBUG

}  // namespace
