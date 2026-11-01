#pragma once

#include <iterator>
#include <list>
#include <optional>
#include <unordered_map>

#include "cache.hpp"

namespace caches {

template <typename KeyT, typename PageT>
class LFU : public ICache<KeyT, PageT> {
 private:
  struct Node {
    KeyT key;
    PageT page;
    unsigned int freq;
  };

  using Bucket = std::list<Node>;
  using ListIt = Bucket::iterator;

 public:
  explicit LFU(const std::size_t capacity) : capacity_(capacity) {}

  LFU(const LFU& other) = delete;
  LFU& operator=(const LFU& other) = delete;

  LFU(LFU&& other) noexcept = default;
  LFU& operator=(LFU&& other) noexcept = default;

  std::optional<PageT> get(const KeyT& key) override {
    auto node_iter = node_iters_.find(key);
    if (node_iter == node_iters_.end()) {
      this->inc_misses();
      return std::nullopt;
    }

    this->inc_hits();
    move_up_node(node_iter->second);
    return node_iter->second->page;
  }

  void put(const KeyT& key, const PageT& page) override {
    if (capacity() == 0) {
      return;
    }

    auto node_iter = node_iters_.find(key);
    if (node_iter != node_iters_.end()) {
      node_iter->second->page = page;
      move_up_node(node_iter->second);
      return;
    }

    if (full()) {
      evict();
    }

    Bucket& bucket = buckets_[1];
    bucket.push_back(Node{key, page, 1});
    node_iters_.emplace(key, std::prev(bucket.end()));
    min_freq_ = 1;
  }

  std::size_t size() const override { return node_iters_.size(); }
  std::size_t capacity() const override { return capacity_; }
  bool empty() const override { return size() == 0; }
  bool full() const { return size() == capacity(); }
  void clear() override {
    this->hits_ = 0;
    this->misses_ = 0;
    min_freq_ = 0;
    buckets_.clear();
    node_iters_.clear();
  }

 private:
  // Move node to bucket with freq + 1
  void move_up_node(ListIt node) {
    unsigned int old_freq = node->freq;
    unsigned int new_freq = old_freq + 1;

    Bucket& old_bucket = buckets_[old_freq];
    Bucket& new_bucket = buckets_[new_freq];

    new_bucket.splice(new_bucket.end(), old_bucket, node_iters_[node->key]);
    node->freq = new_freq;

    if (old_bucket.empty()) {
      buckets_.erase(old_freq);
      min_freq_ = (min_freq_ == old_freq) ? new_freq : min_freq_;
    }
  }

  void evict() {
    auto bucket_iter = buckets_.find(min_freq_);
    if (bucket_iter == buckets_.end() || bucket_iter->second.empty()) {
        // [need for -Wnull-dereference]
        // We shouldn't be able to jmp here, based on the invariant:
        // `evict` is called when `full()` is true
      return;
    }

    Bucket& bucket = bucket_iter->second;

    node_iters_.erase(bucket.front().key);
    bucket.pop_front();
    if (bucket.empty()) {
      buckets_.erase(bucket_iter);
      min_freq_ = buckets_.empty() ? 0 : buckets_.begin()->first;
    }
  }

  std::size_t capacity_ = 0;
  unsigned int min_freq_ = 0;

   /**
   * frequency -> bucket
   *
   * Invariant:
   *  1) There is at most one bucket for any frequency.
   *  2) All nodes in single bucket are linked in LFU order.
   * */
  std::unordered_map<unsigned int, Bucket> buckets_;

   /**
   * key -> node_iter
   * */
  std::unordered_map<KeyT, ListIt> node_iters_;
};

}  // namespace caches
