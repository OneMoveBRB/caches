#pragma once

#include <algorithm>
#include <cassert>
#include <list>
#include <unordered_map>

#include "cache.hpp"

namespace caches {

template <typename KeyT, typename PageT>
class TwoQ final : public ICache<KeyT, PageT> {
 private:
  struct Node {
    KeyT key;
    PageT page;
  };

  using NodeList = std::list<Node>;
  using NodeIt = NodeList::iterator;
  using GhostList = std::list<KeyT>;
  using GhostIt = typename GhostList::iterator;

  struct Entry {
    NodeIt node_iter;
    bool in_hot;
  };

 public:
  explicit TwoQ(std::size_t capacity, std::size_t in_capacity = 0,
                std::size_t out_capacity = 0)
      : in_capacity_(
            resolve_capacity(in_capacity, capacity, kDefaultInDivPart)),
        hot_capacity_(capacity - in_capacity_),
        out_capacity_(
            resolve_capacity(out_capacity, capacity, kDefaultOutDivPart)) {
    assert(capacity != 0);
    assert(in_capacity_ < capacity);
    assert(out_capacity_ < capacity);
  }

  TwoQ(const TwoQ& other) = delete;
  TwoQ& operator=(const TwoQ& other) = delete;

  TwoQ(TwoQ&& other) noexcept = default;
  TwoQ& operator=(TwoQ&& other) noexcept = default;

  std::optional<PageT> get(const KeyT& key) override {
    auto resident_iter = resident_.find(key);
    if (resident_iter == resident_.end()) {
      this->inc_misses();
      return std::nullopt;
    }

    this->inc_hits();

    Entry& entry = resident_iter->second;
    if (entry.in_hot) {
      touch_hot(entry.node_iter);
    } else {
      promote_from_in_to_hot(entry);
    }

    return entry.node_iter->page;
  }

  void put(const KeyT& key, const PageT& page) override {
    if (capacity() == 0) {
      return;
    }

    if (auto resident_iter = resident_.find(key);
        resident_iter != resident_.end()) {
      Entry& entry = resident_iter->second;
      entry.node_iter->page = page;
      if (entry.in_hot) {
        touch_hot(entry.node_iter);
      } else {
        promote_from_in_to_hot(entry);
      }

      return;
    }

    if (auto out_iter = ghosts_.find(key); out_iter != ghosts_.end()) {
      out_items_.erase(out_iter->second);
      ghosts_.erase(key);
      reclaim_hot();
      hot_items_.push_front(Node{key, page});
      resident_.emplace(key, Entry{hot_items_.begin(), true});

      return;
    }

    reclaim_in();

    in_items_.push_front(Node{key, page});
    resident_.emplace(key, Entry{in_items_.begin(), false});
  }

  std::size_t size() const override { return resident_.size(); }
  std::size_t capacity() const override { return in_capacity_ + hot_capacity_; }
  bool empty() const override { return size() == 0; }
  void clear() override {
    this->hits_ = 0;
    this->misses_ = 0;
    in_items_.clear();
    hot_items_.clear();
    out_items_.clear();
    resident_.clear();
    ghosts_.clear();
  }

 private:
  static constexpr std::size_t kDefaultInDivPart = 4;
  static constexpr std::size_t kDefaultOutDivPart = 2;

  bool full_in() const { return in_items_.size() == in_capacity_; }
  bool full_hot() const { return hot_items_.size() == hot_capacity_; }
  bool full_out() const { return out_items_.size() == out_capacity_; }

  bool empty_hot() const { return hot_items_.empty(); }

  static std::size_t ensure_enough_capacity(std::size_t capacity) {
    return std::max<std::size_t>(1, capacity);
  }
  static std::size_t resolve_capacity(std::size_t requested_capacity,
                                      std::size_t total_capacity,
                                      std::size_t div_part) {
    return requested_capacity ?
               requested_capacity :
               ensure_enough_capacity(total_capacity / div_part);
  }

  void reclaim_in() {
    if (!full_in()) {
      return;
    }

    if (full_out()) {
      evict_out();
    }

    evict_in();
  }
  void reclaim_hot() {
    if (!full_hot() || empty_hot()) {
      return;
    }

    evict_hot();
  }

  void evict_in() {
    Node& node = in_items_.back();
    resident_.erase(node.key);
    out_items_.push_front(node.key);
    ghosts_.emplace(node.key, out_items_.begin());
    in_items_.pop_back();
  }
  void evict_hot() {
    resident_.erase(hot_items_.back().key);
    hot_items_.pop_back();
  }
  void evict_out() {
    ghosts_.erase(out_items_.back());
    out_items_.pop_back();
  }

  // move to MRU
  void touch_hot(NodeIt node_iter) {
    hot_items_.splice(hot_items_.begin(), hot_items_, node_iter);
  }

  void promote_from_in_to_hot(Entry& entry) {
    reclaim_hot();
    hot_items_.splice(hot_items_.begin(), in_items_, entry.node_iter);
    entry.node_iter = hot_items_.begin();
    entry.in_hot = true;
  }

  std::size_t in_capacity_;
  std::size_t hot_capacity_;
  std::size_t out_capacity_;

  NodeList in_items_;
  NodeList hot_items_;
  GhostList out_items_;

  std::unordered_map<KeyT, Entry> resident_;
  std::unordered_map<KeyT, GhostIt> ghosts_;
};

}  // namespace caches

#if 0
  static constexpr double kDefaultInRatio = 0.25;
  static constexpr double kDefaultOutRatio = 0.50;
  explicit TwoQ(std::size_t capacity, double in_ratio = kDefaultInRatio,
                double out_ratio = kDefaultOutRatio)
      : capacity_(capacity),
        in_capacity_(std::max<std::size_t>(
           1, static_cast<std::size_t>((double)capacity * in_ratio))),
        out_capacity_(std::max<std::size_t>(
            1, static_cast<std::size_t>((double)capacity * out_ratio))) {}
#endif
