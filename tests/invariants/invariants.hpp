#pragma once

#include <cstddef>
#include <optional>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "caches/arc.hpp"
#include "caches/cache.hpp"
#include "caches/lfu.hpp"
#include "caches/lirs.hpp"
#include "caches/lru.hpp"
#include "caches/two_q.hpp"

namespace caches::test {

using Key = int;
using Page = int;

template <typename CacheT>
struct MinCapacity {
  static constexpr std::size_t value = 0;
};

template <typename KeyT, typename PageT>
struct MinCapacity<TwoQ<KeyT, PageT>> {
  static constexpr std::size_t value = 2;
};

template <typename KeyT, typename PageT>
struct MinCapacity<LIRS<KeyT, PageT>> {
  static constexpr std::size_t value = 2;
};

// ---------------------------------------------------------------------------
// Trace helpers
// ---------------------------------------------------------------------------

inline std::vector<Key> make_random_trace(std::size_t n, int key_space,
                                          std::uint32_t seed) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> dist(0, key_space - 1);
  std::vector<Key> trace;
  trace.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    trace.push_back(dist(rng));
  return trace;
}

inline std::vector<Key> make_evil_trace() {
  std::vector<Key> t;
  for (int round = 0; round < 5; ++round) {
    for (int k = 0; k < 10; ++k)
      t.push_back(k);
    for (int k = 10; k < 15; ++k)
      t.push_back(k);
    t.push_back(0);
    t.push_back(1);
  }
  return t;
}

// ---------------------------------------------------------------------------
// Generic trace runner with per-step invariants
// ---------------------------------------------------------------------------

template <typename CacheT>
void run_trace_checked(CacheT& cache, const std::vector<Key>& trace) {
  std::size_t expected_gets = 0;

  for (const Key& key : trace) {
    const std::size_t before_hits = cache.hits();
    const std::size_t before_misses = cache.misses();

    auto value = cache.get(key);
    ++expected_gets;

    const std::size_t after_hits = cache.hits();
    const std::size_t after_misses = cache.misses();

    const bool hit_advanced =
        (after_hits == before_hits + 1) && (after_misses == before_misses);
    const bool miss_advanced =
        (after_misses == before_misses + 1) && (after_hits == before_hits);
    EXPECT_TRUE(hit_advanced || miss_advanced)
        << "get() must increment exactly one counter by one";

    EXPECT_EQ(after_hits + after_misses, expected_gets)
        << "hits+misses must equal number of get calls";

    EXPECT_LE(cache.size(), cache.capacity()) << "size() exceeded capacity()";
    EXPECT_EQ(cache.empty(), cache.size() == 0)
        << "empty() inconsistent with size()";

    const std::size_t h_before_put = cache.hits();
    const std::size_t m_before_put = cache.misses();
    if (value.has_value()) {
      cache.put(key, *value);
    } else {
      cache.put(key, Page{key});
    }
    EXPECT_EQ(cache.hits(), h_before_put) << "put() must not change hits";
    EXPECT_EQ(cache.misses(), m_before_put) << "put() must not change misses";

    EXPECT_LE(cache.size(), cache.capacity());
    EXPECT_EQ(cache.empty(), cache.size() == 0);
  }
}

template <typename CacheT>
void run_gets_checked(CacheT& cache, const std::vector<Key>& trace) {
  std::size_t expected_gets = 0;
  for (const Key& key : trace) {
    const std::size_t before_hits = cache.hits();
    const std::size_t before_misses = cache.misses();

    (void)before_hits;
    (void)before_misses;

    (void)cache.get(key);
    ++expected_gets;

    const std::size_t after_hits = cache.hits();
    const std::size_t after_misses = cache.misses();

    EXPECT_EQ(after_hits + after_misses, expected_gets);
    EXPECT_LE(cache.size(), cache.capacity());
    EXPECT_EQ(cache.empty(), cache.size() == 0);
  }
}

}  // namespace caches::test
