#pragma once

#include <unordered_map>
#include <list>

#include "cache.hpp"

namespace caches {

template <typename KeyT, typename PageT>
class LRU final : public ICache<KeyT, PageT> {
 private:
  struct Node {
    KeyT key;
    PageT page;
  };

  using ListIt = typename std::list<Node>::iterator;

 public:
  explicit LRU(std::size_t capacity) : capacity_(capacity) {}

  LRU(const LRU& other) = delete;
  LRU& operator=(const LRU& other) = delete;

  LRU(LRU&& other) noexcept = default;
  LRU& operator=(LRU&& other) noexcept = default;

  std::optional<PageT> get(const KeyT& key) override {
    auto cache_iter = idx_map_.find(key);
    if (cache_iter == idx_map_.end()) {
      this->inc_misses();
      return std::nullopt;
    }

    this->inc_hits();
    items_.splice(items_.begin(), items_, cache_iter->second);
    return *cache_iter->second->page;
  }

  void put(const KeyT& key, const PageT& page) override {
    auto cache_iter = idx_map_.find(key);
    if (cache_iter != idx_map_.end()) {
      *cache_iter->second->page = page;
      items_.splice(items_.begin(), items_, cache_iter->second);
      return;
    }

    if (full()) {
      evict();
    }

    items_.push_front(Node{key, page});
    idx_map_[key] = items_.begin();
  }

  bool full() const { return capacity_ == items_.size(); }
  std::size_t size() const override { return items_.size(); }
  std::size_t capacity() const override { return capacity_; }

  void clear() override {
    items_.clear();
    idx_map_.clear();
    this->hits_ = 0;
    this->misses_ = 0;
  }

 private:
  void evict() {
    ListIt last = items_.back();
    idx_map_.erase(last.key);
    items_.pop_back();
  }

  std::size_t capacity_;
  std::list<Node> items_;
  std::unordered_map<KeyT, ListIt> idx_map_;
};

}  // namespace caches
