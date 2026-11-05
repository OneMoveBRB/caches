#pragma once

#include <cassert>
#include <cstddef>
#include <list>
#include <unordered_map>

#include "cache.hpp"

namespace caches {

template <typename KeyT, typename PageT>
class ARC final : public ICache<KeyT, PageT> {
 private:
  using List = std::list<KeyT>;
  using ListIt = typename List::iterator;
  using UMap = std::unordered_map<KeyT, PageT>;
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

 public:
  std::optional<PageT> get(const KeyT& key);
  void put(const KeyT& key, const PageT& page);

  std::size_t size() const;
  std::size_t capacity() const;
  bool empty() const { return size() == 0; }
  void clear();

 private:
  // Helpers
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

  static bool is_resident(Segment segment) {
    return segment == Segment::Top1 || segment == Segment::Top2;
  }

  std::size_t resident_size() const { return top1_.size() + top2_.size(); }

  std::size_t ghost_size() const { return bottom1_.size() + bottom2_.size(); }

  void move_to_mru(UMapIt entries_iter, Segment dest_segment) {
    Entry& entry = entries_iter->second;

    List& src = list_of(entry.segment);
    List& dest = list_of(dest_segment);

    dest.splice(dest.begin(), src, entry.iter);

    entry.segment = dest_segment;
    // entry.iter = dest.begin();
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

  std::size_t capacity_ = 0;
  std::size_t parameter_ = 0;

  List top1_, bottom1_;
  List top2_, bottom2_;

  std::unordered_map<KeyT, Entry> entries_;
};

}  // namespace caches
