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
class CounterInvariant : public ::testing::Test {};
using CacheTypes =
    ::testing::Types<LRU<Key, Page>, LFU<Key, Page>, TwoQ<Key, Page>,
                     LIRS<Key, Page>, ARC<Key, Page>>;
TYPED_TEST_SUITE(CounterInvariant, CacheTypes);

TYPED_TEST(CounterInvariant, GetsOnlyAdvanceCountersByOne) {
  TypeParam cache(3);
  const auto trace = make_random_trace(200, 10, /*seed=*/42);
  run_gets_checked(cache, trace);
  EXPECT_EQ(cache.hits() + cache.misses(), trace.size());
}

TYPED_TEST(CounterInvariant, PutDoesNotTouchCounters) {
  TypeParam cache(3);
  for (int k = 0; k < 50; ++k) {
    cache.put(k, k);
    EXPECT_EQ(cache.hits(), 0u);
    EXPECT_EQ(cache.misses(), 0u);
  }
}

}  // namespace
