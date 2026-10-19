#pragma once
// ClockCache<V, Capacity>: fixed-capacity write-back cache with CLOCK (second-chance) eviction.
//
// A hit sets the slot's reference bit and nothing else: no list is relinked, so a hit costs one
// index probe and one byte write. The eviction hand sweeps the slots; a slot with its reference
// bit set gets it cleared and is passed over once, the first slot without it is the victim. That
// approximates LRU at a fraction of the bookkeeping.
//
// Misses and write-back are the caller's job: put() reports the victim through a callback
// `evict(key, value&, dirty)` BEFORE the slot is reused, so a dirty value can be written to the
// backing store. Nothing here calls back into user code on a hit or allocates after construction.
//
// Slots are marked occupied explicitly. A cache that identifies "never used" by comparing the key
// with a default key (0) will, when key 0 is a real key, erase the live mapping of key 0 the first
// time it recycles an unused slot, losing a dirty write. tests/test_clock_cache.cpp pins this.
//
// flush() writes every dirty entry and keeps it resident (clean). invalidate() drops an entry
// WITHOUT writing it back.
#include "hftkit/ds/open_index.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace hftkit {

template <class V, std::size_t Capacity>
class ClockCache {
  static_assert(std::has_single_bit(Capacity) && Capacity >= 4, "Capacity: power of two >= 4");

 public:
  using Key = std::uint64_t;

  ClockCache()
      : slots_(std::make_unique<Slot[]>(Capacity)),
        free_(std::make_unique<std::uint32_t[]>(Capacity)) {
    // Popped from the back, so slots are first used in index order 0, 1, 2, ...
    for (std::size_t i = 0; i < Capacity; ++i) {
      free_[i] = static_cast<std::uint32_t>(Capacity - 1 - i);
    }
    free_count_ = Capacity;
  }

  // Marks the entry recently used. nullptr on a miss.
  [[nodiscard]] V* find(Key key) noexcept {
    const std::uint32_t idx = index_.find(key);
    if (idx == Index::kNone) return nullptr;
    Slot& s = slots_[idx];
    s.flags |= kRef;
    return &s.value;
  }

  // Does not touch the reference bit.
  [[nodiscard]] const V* peek(Key key) const noexcept {
    const std::uint32_t idx = index_.find(key);
    return idx == Index::kNone ? nullptr : &slots_[idx].value;
  }

  // Inserts or overwrites. `dirty` marks the value as newer than the backing store (a present entry
  // stays dirty once dirty). `evict(key, value&, dirty)` runs for the victim when the cache is
  // full.
  template <class Evict>
  V& put(Key key, V value, bool dirty, Evict&& evict) {
    if (const std::uint32_t idx = index_.find(key); idx != Index::kNone) {
      Slot& s = slots_[idx];
      s.value = std::move(value);
      s.flags = static_cast<std::uint8_t>(s.flags | kRef | (dirty ? kDirty : 0U));
      return s.value;
    }
    std::uint32_t idx;
    if (free_count_ != 0) {
      idx = free_[--free_count_];
    } else {
      idx = pick_victim();
      Slot& v = slots_[idx];
      ++evictions_;
      const bool was_dirty = (v.flags & kDirty) != 0;
      if (was_dirty) ++dirty_evictions_;
      evict(v.key, v.value, was_dirty);
      index_.erase(v.key);
      --size_;
      hand_ = (idx + 1) & kMask;
    }
    Slot& s = slots_[idx];
    s.key = key;
    s.value = std::move(value);
    s.flags = static_cast<std::uint8_t>(kOccupied | (dirty ? kDirty : 0U));
    index_.insert(key, idx);
    ++size_;
    return s.value;
  }

  // Drops an entry without write-back. False if it was not cached.
  bool invalidate(Key key) noexcept {
    const std::uint32_t idx = index_.find(key);
    if (idx == Index::kNone) return false;
    index_.erase(key);
    slots_[idx].flags = 0;
    free_[free_count_++] = idx;
    --size_;
    return true;
  }

  // flush_fn(key, value) for every dirty entry; the entries stay cached and become clean.
  template <class F>
  std::size_t flush(F&& flush_fn) {
    std::size_t n = 0;
    for (std::size_t i = 0; i < Capacity; ++i) {
      Slot& s = slots_[i];
      if ((s.flags & (kOccupied | kDirty)) == (kOccupied | kDirty)) {
        flush_fn(s.key, s.value);
        s.flags = static_cast<std::uint8_t>(s.flags & ~kDirty);
        ++n;
      }
    }
    return n;
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }
  [[nodiscard]] std::uint64_t evictions() const noexcept { return evictions_; }
  [[nodiscard]] std::uint64_t dirty_evictions() const noexcept { return dirty_evictions_; }

 private:
  using Index = OpenIndex<2 * Capacity>;  // load <= 1/2, so it never reaches its 7/8 limit
  static constexpr std::size_t kMask = Capacity - 1;
  static constexpr std::uint8_t kOccupied = 1;
  static constexpr std::uint8_t kRef = 2;
  static constexpr std::uint8_t kDirty = 4;

  struct Slot {
    Key key{};
    V value{};
    std::uint8_t flags = 0;
  };

  // Only called when every slot is occupied. Terminates within two sweeps.
  [[nodiscard]] std::uint32_t pick_victim() noexcept {
    for (;;) {
      Slot& s = slots_[hand_];
      const std::uint32_t at = static_cast<std::uint32_t>(hand_);
      if ((s.flags & kRef) != 0) {
        s.flags = static_cast<std::uint8_t>(s.flags & ~kRef);
        hand_ = (hand_ + 1) & kMask;
      } else {
        return at;
      }
    }
  }

  Index index_;
  std::unique_ptr<Slot[]> slots_;
  std::unique_ptr<std::uint32_t[]> free_;
  std::size_t free_count_ = 0;
  std::size_t size_ = 0;
  std::size_t hand_ = 0;
  std::uint64_t evictions_ = 0;
  std::uint64_t dirty_evictions_ = 0;
};

}  // namespace hftkit
