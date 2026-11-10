#include "invariants.hpp"

namespace {

using namespace caches;
using namespace caches::test;

template <typename CacheT>
class SizeInvariant : public ::testing::Test {};
using CacheTypes =
    ::testing::Types<LRU<Key, Page>, LFU<Key, Page>, TwoQ<Key, Page>,
                     LIRS<Key, Page>, ARC<Key, Page>>;
TYPED_TEST_SUITE(SizeInvariant, CacheTypes);

TYPED_TEST(SizeInvariant, HoldsAfterEveryStepOnEvilTrace) {
  for (std::size_t cap : {1u, 2u, 3u, 5u, 10u}) {
    if (cap < MinCapacity<TypeParam>::value)
      continue;
    TypeParam cache(cap);
    run_trace_checked(cache, make_evil_trace());
  }
}

TYPED_TEST(SizeInvariant, FillsUpOnRepeatingWorkload) {
  TypeParam cache(4);
  std::size_t max_size = 0;
  for (int round = 0; round < 10; ++round) {
    for (int k = 0; k < 4; ++k)
      cache.put(k, k);
    for (int k = 0; k < 4; ++k)
      (void)cache.get(k);
    max_size = std::max(max_size, cache.size());
    ASSERT_LE(cache.size(), cache.capacity());
  }

  // Every cache should reach at least capacity-1 at some point
  EXPECT_GE(max_size, cache.capacity() - 1);
}

TYPED_TEST(SizeInvariant, NeverExceedsCapacityUnderHeavyInsert) {
  TypeParam cache(4);
  for (int k = 0; k < 1000; ++k) {
    cache.put(k, k);
    ASSERT_LE(cache.size(), cache.capacity());
  }
}

}  // namespace
