#include "invariants.hpp"

#include "caches/arc.hpp"
#include "caches/lfu.hpp"
#include "caches/lirs.hpp"
#include "caches/lru.hpp"
#include "caches/two_q.hpp"

namespace {

using namespace caches;
using namespace caches::test;

template <typename CacheT>
class ClearSemantics : public ::testing::Test {};
using CacheTypes =
    ::testing::Types<LRU<Key, Page>, LFU<Key, Page>, TwoQ<Key, Page>,
                     LIRS<Key, Page>, ARC<Key, Page>>;
TYPED_TEST_SUITE(ClearSemantics, CacheTypes);

TYPED_TEST(ClearSemantics, EmptiesCacheAndResetsCounters) {
  TypeParam cache(3);
  const auto trace = make_random_trace(50, 5, /*seed=*/7);
  run_gets_checked(cache, trace);
  ASSERT_GT(cache.hits() + cache.misses(), 0u);

  cache.clear();

  EXPECT_EQ(cache.size(), 0u);
  EXPECT_TRUE(cache.empty());
  EXPECT_EQ(cache.hits(), 0u);     // contract: clear() resets counters
  EXPECT_EQ(cache.misses(), 0u);

  for (int k = 0; k < 5; ++k) {
    EXPECT_FALSE(cache.get(k).has_value());
  }
  EXPECT_EQ(cache.misses(), 5u);
}

TYPED_TEST(ClearSemantics, DoubleClearIsSafe) {
  TypeParam cache(3);
  cache.put(1, 1);
  cache.clear();
  cache.clear();
  EXPECT_TRUE(cache.empty());
}

}  // namespace
