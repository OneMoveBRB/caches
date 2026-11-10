#include "invariants.hpp"

#include "caches/belady.hpp"

namespace {

using namespace caches;
using namespace caches::test;

template <typename CacheT>
void expect_belady_ge(std::size_t capacity, const std::vector<Key>& trace) {
  if (capacity < MinCapacity<CacheT>::value)
    return;

  OptimalCache<Key> opt(capacity, trace);
  opt.simulate();
  const std::size_t opt_hits = opt.stats().hits;

  CacheT cache(capacity);
  run_gets_checked(cache, trace);
  const std::size_t hits = cache.hits();

  EXPECT_LE(hits, opt_hits)
      << "Online cache beat Belady: hits=" << hits << " opt_hits=" << opt_hits
      << " capacity=" << capacity;
}

TEST(BeladyOptimality, OnlineNeverBeatsBeladyOnRandomTraces) {
  for (std::uint32_t seed : {1u, 2u, 3u, 7u, 42u}) {
    const auto trace = make_random_trace(300, 12, seed);
    for (std::size_t cap : {1u, 2u, 3u, 5u, 8u}) {
      expect_belady_ge<LRU<Key, Page>>(cap, trace);
      expect_belady_ge<LFU<Key, Page>>(cap, trace);
      expect_belady_ge<TwoQ<Key, Page>>(cap, trace);
      expect_belady_ge<LIRS<Key, Page>>(cap, trace);
      expect_belady_ge<ARC<Key, Page>>(cap, trace);
    }
  }
}

TEST(BeladyOptimality, OnlineNeverBeatsBeladyOnEvilTrace) {
  const auto trace = make_evil_trace();
  for (std::size_t cap : {1u, 2u, 3u, 5u}) {
    expect_belady_ge<LRU<Key, Page>>(cap, trace);
    expect_belady_ge<LFU<Key, Page>>(cap, trace);
    expect_belady_ge<TwoQ<Key, Page>>(cap, trace);
    expect_belady_ge<LIRS<Key, Page>>(cap, trace);
    expect_belady_ge<ARC<Key, Page>>(cap, trace);
  }
}

TEST(BeladyOptimality, OwnCountersConsistent) {
  const auto trace = make_random_trace(200, 10, /*seed=*/5);
  OptimalCache<Key> opt(3, trace);
  opt.simulate();

  EXPECT_EQ(opt.stats().hits + opt.stats().misses, trace.size());
  EXPECT_EQ(opt.processed_requests(), trace.size());
  EXPECT_LE(opt.size(), opt.capacity());
}

}  // namespace
