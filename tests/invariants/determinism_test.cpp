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
class Determinism : public ::testing::Test {};
using CacheTypes =
    ::testing::Types<LRU<Key, Page>, LFU<Key, Page>, TwoQ<Key, Page>,
                     LIRS<Key, Page>, ARC<Key, Page>>;
TYPED_TEST_SUITE(Determinism, CacheTypes);

TYPED_TEST(Determinism, TwoFreshCachesProduceSameCounters) {
  const auto trace = make_random_trace(500, 20, /*seed=*/123);

  TypeParam a(5);
  TypeParam b(5);
  run_gets_checked(a, trace);
  run_gets_checked(b, trace);

  EXPECT_EQ(a.hits(), b.hits());
  EXPECT_EQ(a.misses(), b.misses());
}

// Critical for adaptive caches (ARC, LIRS): clear() must reset the
// adaptive state, so a second run over the same trace on the same
// object matches a fresh object.
TYPED_TEST(Determinism, ClearRestoresFreshState) {
  const auto trace = make_random_trace(500, 20, /*seed=*/321);

  TypeParam reused(5);
  run_gets_checked(reused, trace);
  reused.clear();
  run_gets_checked(reused, trace);

  TypeParam fresh(5);
  run_gets_checked(fresh, trace);

  EXPECT_EQ(reused.hits(), fresh.hits());
  EXPECT_EQ(reused.misses(), fresh.misses());
}

}  // namespace
