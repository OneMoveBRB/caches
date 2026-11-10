#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace caches {

template <typename KeyT>
class OptimalCache {
 public:
  enum class Result {
    Hit = 0,
    Miss = 1
  };

  struct Statistics {
    std::size_t hits = 0;
    std::size_t misses = 0;

    std::size_t total() const noexcept { return hits + misses; }

    double hit_rate() const noexcept {
      return total() == 0 ?
                 0.0 :
                 static_cast<double>(hits) / static_cast<double>(total());
    }

    double miss_rate() const noexcept {
      return total() == 0 ?
                 0.0 :
                 static_cast<double>(misses) / static_cast<double>(total());
    }
  };

  struct AccessResult {
    Result result;
    std::optional<KeyT> evicted;
  };

  explicit OptimalCache(std::size_t capacity, std::vector<KeyT> requests)
      : capacity_(capacity), requests_(std::move(requests)) {
    next_use_ = calculate_next_use(requests_);
  }

  // Process a single request
  // Requests must arrive in the original order
  AccessResult access(const KeyT& key) {
    assert(position_ < requests_.size() && "No more planned requests");

    const std::size_t next = next_use_[position_++];

    auto it = cache_.find(key);

    // Cache Hit
    if (it != cache_.end()) {
      eviction_order_.erase({it->second.next_use, it->second.id});
      it->second.next_use = next;
      eviction_order_.insert({next, it->second.id});

      ++stats_.hits;

      return {Result::Hit, std::nullopt};
    }

    // Cache Miss
    ++stats_.misses;

    if (capacity() == 0) {
      return {Result::Miss, std::nullopt};
    }

    std::optional<KeyT> evicted;

    // If the cache is full, evict the optimal element
    if (cache_.size() == capacity_) {
      auto victim = std::prev(eviction_order_.end());

      const std::uint64_t id = victim->second;

      auto key_it = id_to_key_.find(id);
      if (key_it == id_to_key_.end()) {
        // [need for -Werror=null-dereference]
        // We shouldn't be able to jmp here, based on the invariant:
        // eviction is triggered when the cache is full
        return {Result::Miss, std::nullopt};
      }

      evicted = key_it->second;

      cache_.erase(*evicted);
      id_to_key_.erase(key_it);
      eviction_order_.erase(victim);
    }

    const std::uint64_t id = next_id_++;

    cache_.emplace(key, CacheEntry{next, id});
    id_to_key_.emplace(id, key);
    eviction_order_.insert({next, id});

    return {Result::Miss, std::move(evicted)};
  }

  // Reset state before a new experiment
  void reset() {
    cache_.clear();
    id_to_key_.clear();
    eviction_order_.clear();

    position_ = 0;
    next_id_ = 0;

    stats_ = {0};
  }

  bool contains(const KeyT& key) const {
    return cache_.find(key) != cache_.end();
  }

  std::size_t size() const noexcept { return cache_.size(); }
  std::size_t capacity() const noexcept { return capacity_; }

  std::size_t processed_requests() const noexcept { return position_; }
  const Statistics& stats() const noexcept { return stats_; }
  bool has_next_request() const noexcept {
    return position_ < requests_.size();
  }

  // Full simulation of the sequence.
  std::vector<AccessResult> simulate() {
    reset();

    std::vector<AccessResult> results;
    results.reserve(requests_.size());

    for (const KeyT& key : requests_) {
      results.push_back(access(key));
    }

    return results;
  }

 private:
  static constexpr std::size_t kInf = std::numeric_limits<std::size_t>::max();

  struct CacheEntry {
    std::size_t next_use;
    std::uint64_t id;
  };

  // (next_use, unique_id)
  using EvictionSet = std::set<std::pair<std::size_t, std::uint64_t>>;
  using CacheMap = std::unordered_map<KeyT, CacheEntry>;

  static std::vector<std::size_t> calculate_next_use(
      const std::vector<KeyT>& requests) {
    const std::size_t n = requests.size();

    std::vector<std::size_t> result(n, kInf);

    std::unordered_map<KeyT, std::size_t> last;
    last.reserve(n);

    for (std::size_t i = n; i-- > 0;) {
      const KeyT& key = requests[i];

      auto it = last.find(key);

      if (it != last.end()) {
        result[i] = it->second;
        it->second = i;
      } else {
        last.emplace(key, i);
      }
    }

    return result;
  }

  std::size_t capacity_;

  std::vector<KeyT> requests_;
  std::vector<std::size_t> next_use_;

  CacheMap cache_;

  // Allows retrieving a Key by its unique ID
  std::unordered_map<std::uint64_t, KeyT> id_to_key_;

  // Elements are sorted by their next use
  EvictionSet eviction_order_;

  std::size_t position_ = 0;
  std::uint64_t next_id_ = 0;

  Statistics stats_;
};

}  // namespace caches
