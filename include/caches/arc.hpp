#pragma once

#include <algorithm>
#include <cassert>
#include <list>
#include <unordered_map>
#include <utility>

#include "cache.hpp"

namespace caches {

template <typename KeyT, typename PageT>
class ARC final : public ICache<KeyT, PageT> {
 private:
  struct Entry;

  using List = std::list<KeyT>;
  using ListIt = typename List::iterator;
  using UMap = std::unordered_map<KeyT, Entry>;
  using UMapIt = typename UMap::iterator;

  enum class Segment {
    Top1 = 0,
    Top2 = 1,
    Bottom1 = 2,
    Bottom2 = 3
  };

  struct Entry {
    Segment segment;
    ListIt iter;
    std::optional<PageT> page;
  };

#ifndef NDEBUG
  bool check_invariants() const {
    if (parameter_ > capacity()) {
      return false;
    }

    if (resident_size() > capacity()) {
      return false;
    }

    if (top1_.size() + bottom1_.size() > capacity()) {
      return false;
    }

    if (resident_size() + ghost_size() > 2 * capacity()) {
      return false;
    }

    if (entries_.size() != resident_size() + ghost_size()) {
      return false;
    }

    return check_list(top1_, Segment::Top1) && check_list(top2_, Segment::Top2)
           && check_list(bottom1_, Segment::Bottom1)
           && check_list(bottom2_, Segment::Bottom2);
  }
#endif // !NDEBUG

 public:
  explicit ARC(std::size_t capacity) : capacity_(capacity) {}

  ARC(const ARC& other) = delete;
  ARC(ARC&& other) noexcept
      : capacity_(other.capacity_),
        parameter_(std::exchange(other.parameter_, 0)),
        top1_(std::move(other.top1_)),
        top2_(std::move(other.top2_)),
        bottom1_(std::move(other.bottom1_)),
        bottom2_(std::move(other.bottom2_)),
        entries_(std::move(other.entries_)) {
    this->hits_ = std::exchange(other.hits_, 0);
    this->misses_ = std::exchange(other.misses_, 0);
  }

  ARC& operator=(const ARC& other) = delete;
  ARC& operator=(ARC&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    ARC temp_arc(std::move(other));
    swap(temp_arc);

    return *this;
  }

  std::optional<PageT> get(const KeyT& key) override {
    auto entries_iter = entries_.find(key);

    if (entries_iter == entries_.end()) {
      this->inc_misses();
      return std::nullopt;
    }

    Entry& entry = entries_iter->second;

    // Ghost hit: the page is not in memory
    if (!is_resident(entry.segment)) {
      this->inc_misses();
      return std::nullopt;
    }

    this->inc_hits();

    // Cache hit: Top1 -> Top2 or Top2 -> MRU Top2
    move_to_mru(entries_iter, Segment::Top2);

    return entry.page;
  }

  void put(const KeyT& key, const PageT& page) override {
    if (capacity() == 0) {
      return;
    }

    auto entries_iter = entries_.find(key);

    // New key
    if (entries_iter == entries_.end()) {
      insert_new(key, page);

      return;
    }

    Entry& entry = entries_iter->second;

    // Resident hit
    if (is_resident(entry.segment)) {
      entry.page = page;
      move_to_mru(entries_iter, Segment::Top2);

      return;
    }

    // Ghost hit
    if (entry.segment == Segment::Bottom1) {
    // Ghost hit: Bottom1
      adapt_from_bottom1();
    } else {
    // Ghost hit: Bottom2
      adapt_from_bottom2();
    }

    if (resident_size() == capacity()) {
      replace(key);
    }

    entry.page = page;
    move_to_mru(entries_iter, Segment::Top2);

    return;
  }

  std::size_t size() const override { return resident_size(); }
  std::size_t capacity() const override { return capacity_; }
  bool empty() const override { return size() == 0; }
  void clear() override {
    this->hits_ = 0;
    this->misses_ = 0;

    parameter_ = 0;

    top1_.clear();
    top2_.clear();
    bottom1_.clear();
    bottom2_.clear();

    entries_.clear();
  }

  void swap(ARC& other) noexcept {
    std::swap(this->hits_, other.hits_);
    std::swap(this->misses_, other.misses_);

    std::swap(capacity_, other.capacity_);
    std::swap(parameter_, other.parameter_);
    std::swap(top1_, other.top1_);
    std::swap(top2_, other.top2_);
    std::swap(bottom1_, other.bottom1_);
    std::swap(bottom2_, other.bottom2_);
    std::swap(entries_, other.entries_);
  }

 private:
  List& list_of(Segment segment) {
    switch (segment) {
      case Segment::Top1:
        return top1_;

      case Segment::Top2:
        return top2_;

      case Segment::Bottom1:
        return bottom1_;

      case Segment::Bottom2:
        return bottom2_;
    }

    assert(false && "Invalid ARC segment");
    return top1_;
  }

  const List& list_of(Segment segment) const {
    switch (segment) {
      case Segment::Top1:
        return top1_;

      case Segment::Top2:
        return top2_;

      case Segment::Bottom1:
        return bottom1_;

      case Segment::Bottom2:
        return bottom2_;
    }

    assert(false && "Invalid ARC segment");
    return top1_;
  }

  std::size_t resident_size() const { return top1_.size() + top2_.size(); }
  std::size_t ghost_size() const { return bottom1_.size() + bottom2_.size(); }

  static bool is_resident(Segment segment) {
    return segment == Segment::Top1 || segment == Segment::Top2;
  }

  void move_to_mru(UMapIt it, Segment dest_segment) {
    Entry& entry = it->second;

    List& src = list_of(entry.segment);
    List& dest = list_of(dest_segment);

    dest.splice(dest.begin(), src, entry.iter);

    entry.segment = dest_segment;
  }

  void remove_ghost_lru(Segment segment) {
    List& ghost = list_of(segment);

    if (ghost.empty()) {
      return;
    }

    const KeyT& key = ghost.back();

    entries_.erase(key);
    ghost.pop_back();
  }

  void remove_top1_lru() {
    assert(!top1_.empty());

    entries_.erase(top1_.back());
    top1_.pop_back();
  }

  void evict(Segment src_segment, Segment dest_segment) {
    List& src = list_of(src_segment);

    assert(!src.empty());

    auto it = entries_.find(src.back());

    assert(it != entries_.end());

    move_to_mru(it, dest_segment);

    // Ghost contains only the key
    it->second.page.reset();
  }

  void replace(const KeyT& key) {
    auto entries_iter = entries_.find(key);

    const bool in_bottom2 = entries_iter != entries_.end()
                            && entries_iter->second.segment == Segment::Bottom2;

    const bool choose_top1 = !top1_.empty()
                             && ((in_bottom2 && top1_.size() == parameter_)
                                 || top1_.size() > parameter_);

    if (choose_top1) {
      evict(Segment::Top1, Segment::Bottom1);
    } else if (!top2_.empty()) {
      evict(Segment::Top2, Segment::Bottom2);
    } else if (!top1_.empty()) {
      // just in case: protection against a degenerate state
      assert(false && "ARC: degenerate state in replace()");
      evict(Segment::Top1, Segment::Bottom1);
    }
  }

  void adapt_from_bottom1() {
    const std::size_t b1_sz = bottom1_.size();
    const std::size_t b2_sz = bottom2_.size();

    const std::size_t delta =
        std::max<std::size_t>(1, b2_sz / std::max<std::size_t>(1, b1_sz));

    parameter_ += std::min(delta, capacity() - parameter_);
  }

  void adapt_from_bottom2() {
    const std::size_t b1_sz = bottom1_.size();
    const std::size_t b2_sz = bottom2_.size();

    const std::size_t delta =
        std::max<std::size_t>(1, b1_sz / std::max<std::size_t>(1, b2_sz));

    parameter_ = delta >= parameter_ ? 0 : parameter_ - delta;
  }

  void insert_new(const KeyT& key, const PageT& page) {
    // Case 1: |Top1| + |Bottom1| == capacity
    if (top1_.size() + bottom1_.size() == capacity()) {
      if (top1_.size() < capacity()) {
        remove_ghost_lru(Segment::Bottom1);
        replace(key);
      } else {
        remove_top1_lru();
      }
    }

    // Case 2: |Top1| + |Bottom1| < capacity
    else if (top1_.size() + bottom1_.size() < capacity()
             && resident_size() + ghost_size() >= capacity()) {
      if (resident_size() + ghost_size() >= 2 * capacity()) {
        remove_ghost_lru(Segment::Bottom2);
      }

      replace(key);
    }

    // Insert into Top1
    top1_.push_front(key);

    try {
      entries_.emplace(key, Entry{Segment::Top1, top1_.begin(), page});
    } catch (...) {
      top1_.pop_front();
      throw;
    }
  }

#ifndef NDEBUG
  bool check_list(const List& list, Segment expected) const {
    for (auto it = list.begin(); it != list.end(); ++it) {
      auto map_it = entries_.find(*it);

      if (map_it == entries_.end()) {
        return false;
      }

      const Entry& entry = map_it->second;

      if (entry.segment != expected) {
        return false;
      }

      if (entry.iter != it) {
        return false;
      }

      if (is_resident(expected) != entry.page.has_value()) {
        return false;
      }
    }

    return true;
  }
#endif // !NDEBUG

  std::size_t capacity_ = 0;
  std::size_t parameter_ = 0;

  List top1_;
  List top2_;
  List bottom1_;
  List bottom2_;

  UMap entries_;
};

}  // namespace caches
