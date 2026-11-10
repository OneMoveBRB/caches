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
class PutGetConsistency : public ::testing::Test {};
using CacheTypes =
    ::testing::Types<LRU<Key, Page>, LFU<Key, Page>, TwoQ<Key, Page>,
                     LIRS<Key, Page>, ARC<Key, Page>>;
TYPED_TEST_SUITE(PutGetConsistency, CacheTypes);

TYPED_TEST(PutGetConsistency, PutThenGetReturnsValueAndHits) {
  for (std::size_t cap : {2u, 3u, 10u}) {
    TypeParam cache(cap);
    for (int k = 0; k < 20; ++k) {
      const Page value = k * 7 + 3;
      cache.put(k, value);

      const std::size_t hits_before = cache.hits();
      auto got = cache.get(k);
      ASSERT_TRUE(got.has_value()) << "key=" << k << " cap=" << cap;
      EXPECT_EQ(*got, value);
      EXPECT_EQ(cache.hits(), hits_before + 1)
          << "put(k); get(k) must be a hit";
    }
  }
}

}  // namespace
