#include "invariants.hpp"

namespace {

using namespace caches;
using namespace caches::test;

template <typename CacheT>
class EdgeCases : public ::testing::Test {};
using CacheTypes =
    ::testing::Types<LRU<Key, Page>, LFU<Key, Page>, TwoQ<Key, Page>,
                     LIRS<Key, Page>, ARC<Key, Page>>;
TYPED_TEST_SUITE(EdgeCases, CacheTypes);

TYPED_TEST(EdgeCases, ZeroCapacityAlwaysMisses) {
  if constexpr (MinCapacity<TypeParam>::value > 0) {
    GTEST_SKIP() << "Type requires capacity >= "
                 << MinCapacity<TypeParam>::value;
  }
  TypeParam cache(0);
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_TRUE(cache.empty());

  for (int k = 0; k < 20; ++k) {
    auto got = cache.get(k);
    EXPECT_FALSE(got.has_value());
    cache.put(k, k);
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_TRUE(cache.empty());
  }
  EXPECT_EQ(cache.hits(), 0u);
  EXPECT_EQ(cache.misses(), 20u);
}

TYPED_TEST(EdgeCases, CapacityOneEvictsPrevious) {
  if constexpr (MinCapacity<TypeParam>::value > 1) {
    GTEST_SKIP() << "Type requires capacity >= "
                 << MinCapacity<TypeParam>::value;
  }
  TypeParam cache(1);
  cache.put(1, 100);
  auto v1 = cache.get(1);
  ASSERT_TRUE(v1.has_value());
  EXPECT_EQ(*v1, 100);

  cache.put(2, 200);
  auto v1_again = cache.get(1);
  EXPECT_FALSE(v1_again.has_value())
      << "capacity 1 must evict key 1 after put(2)";
  auto v2 = cache.get(2);
  ASSERT_TRUE(v2.has_value());
  EXPECT_EQ(*v2, 200);
}

TYPED_TEST(EdgeCases, EmptyTraceHasZeroRates) {
  TypeParam cache(3);
  EXPECT_EQ(cache.hits(), 0u);
  EXPECT_EQ(cache.misses(), 0u);
}

}  // namespace
