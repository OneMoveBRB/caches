#pragma once

#include <cassert>
#include <cstddef>
#include <iterator>
#include <list>
#include <optional>
#include <unordered_map>
#include "cache.hpp"

namespace caches {

template <typename KeyT, typename PageT>
class LIRS final : public ICache<KeyT, PageT> {
 private:
  struct Entry;
  using List = std::list<Entry*>;
  using ListIt = List::iterator;

  struct Entry {
    KeyT key;
    std::optional<PageT> value;
    bool resident = false;
    bool lir = false;
    bool in_stack = false;
    bool in_queue = false;
    ListIt stack_iter;
    ListIt queue_iter;
  };

 public:
  std::optional<PageT> get(const KeyT& key) override { return std::nullopt; }
  void put(const KeyT& key, const PageT& page) override {}

  std::size_t size() const override { return resident_count_; }
  std::size_t capacity() const override { return capacity_; }
  bool empty() const override { return size() == 0; }
  void clear() override {}

 private:

  void stack_push_front(Entry& entry) {
    assert(!entry.in_stack);

    stack_.push_front(&entry);
    entry.in_stack = true;
    entry.stack_iter = stack_.begin();
  }

  void stack_move_to_front(Entry& entry) {
    assert(entry.in_stack);

    stack_.splice(stack_.begin(), stack_, entry.stack_iter);
    entry.stack_iter = stack_.begin();
  }

  void stack_remove(Entry& entry) {
    assert(entry.in_stack);

    stack_.erase(entry.stack_iter);
    entry.in_stack = false;
  }

  void queue_push_back(Entry& entry) {
    assert(!entry.in_queue);

    queue_.push_back(&entry);
    entry.in_queue = true;
    entry.queue_iter = std::prev(queue_.end());
  }

  void queue_move_to_back(Entry& entry) {
    assert(entry.in_queue);

    queue_.splice(queue_.end(), queue_, entry.queue_iter);
    entry.queue_iter = std::prev(queue_.end());
  }

  void queue_remove(Entry& entry) {
    assert(entry.in_queue);

    queue_.erase(entry.queue_iter);
    entry.in_queue = false;
  }

  std::size_t capacity_;
  std::size_t lir_size_;
  std::size_t lir_capacity_;
  std::size_t resident_count_;

  List stack_;
  List queue_;

  std::unordered_map<KeyT, Entry> entries_;
};

}  // namespace caches
