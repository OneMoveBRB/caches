#pragma once

#include <cassert>
#include <list>
#include <unordered_map>

#include "cache.hpp"

namespace caches {

template <typename KeyT, typename PageT>
class LIRS final : public ICache<KeyT, PageT> {
 private:
  struct Entry;
  using List = std::list<Entry*>;
  using ListIt = typename List::iterator;

  struct Entry {
    explicit Entry(const KeyT& ukey) : key(ukey) {}

    KeyT key;
    std::optional<PageT> page;
    bool resident = false;
    bool lir = false;
    bool in_stack = false;
    bool in_queue = false;
    ListIt stack_iter{};
    ListIt queue_iter{};
  };

 public:
  explicit LIRS(std::size_t capacity, std::size_t lir_capacity)
      : capacity_(capacity),
        lir_size_(0),
        lir_capacity_(lir_capacity),
        resident_size_(0) {
    assert(capacity >= 2);
    assert(lir_capacity != 0);
    assert(lir_capacity < capacity);
  }

  LIRS(const LIRS& other) = delete;
  LIRS& operator=(const LIRS& other) = delete;

  LIRS(LIRS&& other) noexcept = default;
  LIRS& operator=(LIRS&& other) noexcept = default;

  std::optional<PageT> get(const KeyT& key) override {
    auto entry_iter = entries_.find(key);
    if (entry_iter == entries_.end() || !entry_iter->second.resident) {
      this->inc_misses();
      return std::nullopt;
    }

    this->inc_hits();

    Entry& entry = entry_iter->second;
    touch_resident(entry);

    return entry.page;
  }

  void put(const KeyT& key, const PageT& page) override {
    auto it = entries_.find(key);

    if (it != entries_.end()) {
      Entry& entry = it->second;

      if (entry.resident) {
        entry.page.emplace(page);
        touch_resident(entry);

        return;
      }

      assert(!entry.lir && entry.in_stack && !entry.in_queue);

      reclaim_queue();
      entry.page.emplace(page);
      entry.resident = true;
      ++resident_size_;
      promote_stack_hir_to_lir(entry);

      return;
    }

    reclaim_queue();
    auto [entry_iter, inserted] = entries_.try_emplace(key, key);
    assert(inserted);

    Entry& entry = entry_iter->second;
    entry.page.emplace(page);
    entry.resident = true;
    ++resident_size_;

    // During warm-up, fill the LIR set first. Afterwards, new pages are HIR
    if (lir_size_ < lir_capacity_) {
      entry.lir = true;
      ++lir_size_;
      list_push_front(stack_, entry, &Entry::stack_iter, &Entry::in_stack);
    } else {
      entry.lir = false;
      list_push_front(stack_, entry, &Entry::stack_iter, &Entry::in_stack);
      list_push_front(queue_, entry, &Entry::queue_iter, &Entry::in_queue);
    }

    prune_stack();

    return;
  }

  std::size_t size() const override { return resident_size_; }
  std::size_t capacity() const override { return capacity_; }
  bool empty() const override { return size() == 0; }
  void clear() override {
    this->hits_ = 0;
    this->misses_ = 0;
    lir_size_ = 0;
    resident_size_ = 0;
    stack_.clear();
    queue_.clear();
    entries_.clear();
  }

 private:
  void list_push_front(List& list, Entry& entry,
                       ListIt Entry::* list_iter_member,
                       bool Entry::* in_list_member) {
    assert(!(entry.*in_list_member));

    list.push_front(&entry);
    entry.*list_iter_member = list.begin();
    entry.*in_list_member = true;
  }

  void list_move_to_front(List& list, Entry& entry,
                          ListIt Entry::* list_iter_member,
                          bool Entry::* in_list_member) {
    assert(entry.*in_list_member);
    (void)in_list_member;

    list.splice(list.begin(), list, entry.*list_iter_member);
    entry.*list_iter_member = list.begin();
  }

  void list_remove(List& list, Entry& entry, ListIt Entry::* list_iter_member,
                   bool Entry::* in_list_member) {
    assert(entry.*in_list_member);

    list.erase(entry.*list_iter_member);
    entry.*in_list_member = false;
  }

  Entry& list_pop_back(List& list, bool Entry::* in_list_member) {
    assert(!list.empty());

    Entry* entry = list.back();
    assert(entry->*in_list_member);
    list.pop_back();
    entry->*in_list_member = false;

    return *entry;
  }

  void reclaim_queue() {
    if (size() < capacity()) {
      return;
    }

    evict_queue();
  }

  void evict_queue() {
    assert(!queue_.empty());

    Entry& entry = list_pop_back(queue_, &Entry::in_queue);

    entry.resident = false;
    entry.page.reset();
    --resident_size_;

    if (!entry.in_stack) {
      entries_.erase(entry.key);
    }
  }

  // Demote the LIR at the bottom of S to resident HIR and push it to Q front
  void demote_bottom_lir_to_rhir() {
    assert(!stack_.empty());

    Entry& old_lir = list_pop_back(stack_, &Entry::in_stack);
    assert(old_lir.lir && old_lir.resident && !old_lir.in_queue);

    old_lir.lir = false;
    --lir_size_;

    // A newly demoted HIR page is the newest entry in Q
    list_push_front(queue_, old_lir, &Entry::queue_iter, &Entry::in_queue);
  }

  // Promote an HIR already represented in S (resident or ghost) to LIR
  void promote_stack_hir_to_lir(Entry& entry) {
    assert(entry.in_stack && !entry.lir && entry.resident);

    if (entry.in_queue) {
      list_remove(queue_, entry, &Entry::queue_iter, &Entry::in_queue);
    }  // Resident HIR leaves Q when promoted.

    list_move_to_front(stack_, entry, &Entry::stack_iter, &Entry::in_stack);
    entry.lir = true;
    ++lir_size_;

    if (lir_size_ > lir_capacity_) {
      demote_bottom_lir_to_rhir();
    }

    prune_stack();
  }

  void touch_resident(Entry& entry) {
    assert(entry.resident && entry.page.has_value());

    if (entry.lir) {
      // LIR hit: move to MRU of S.
      assert(entry.in_stack && !entry.in_queue);

      list_move_to_front(stack_, entry, &Entry::stack_iter, &Entry::in_stack);
      prune_stack();

      return;
    }

    if (entry.in_stack) {
      // Resident HIR in S: promote to LIR, demote the oldest LIR
      assert(entry.in_queue);

      promote_stack_hir_to_lir(entry);

      return;
    }

    // Resident HIR in Q but not in S: it remains HIR, but becomes recent
    assert(entry.in_queue);

    list_move_to_front(queue_, entry, &Entry::queue_iter, &Entry::in_queue);
    list_push_front(stack_, entry, &Entry::stack_iter, &Entry::in_stack);
    prune_stack();
  }

  // Remove HIR entries from the LRU end of S until its bottom is LIR
  // Resident HIR removed from S remains in Q
  // Non-Resident HIR removed from S is forgotten
  void prune_stack() {
    while (!stack_.empty() && !stack_.back()->lir) {
      Entry& entry = list_pop_back(stack_, &Entry::in_stack);

      if (!entry.resident) {
        entries_.erase(entry.key);
      }
    }
  }

  std::size_t capacity_;
  std::size_t lir_size_ = 0;
  std::size_t lir_capacity_;
  std::size_t resident_size_ = 0;

  List stack_;
  List queue_;

  std::unordered_map<KeyT, Entry> entries_;
};

}  // namespace caches
