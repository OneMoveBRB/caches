#pragma once

#include <cstddef>
#include <optional>

namespace caches {

template <typename KeyT, typename PageT>
class ICache {
 public:
  virtual ~ICache() = default;

  virtual std::optional<PageT> get(const KeyT& key) = 0;
  virtual void put(const KeyT& key, const PageT& page) = 0;

  virtual std::size_t size() const = 0;
  virtual std::size_t capacity() const = 0;
  virtual bool empty() const = 0;
  virtual void clear() = 0;

  std::size_t hits() const { return hits_; }
  std::size_t misses() const { return misses_; }

 protected:
  void inc_hits() { ++hits_; }
  void inc_misses() { ++misses_; }

  std::size_t hits_ = 0;
  std::size_t misses_ = 0;
};

}  // namespace caches
