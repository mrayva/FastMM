#pragma once
// OpenIndex<Capacity>: fixed-capacity open-addressing map, uint64 key -> uint32 value.
//
//   * linear probing, Fibonacci hash on the TOP bits (hash/fib.hpp);
//   * erase is a backward shift: no tombstones, so lookups never degrade with churn;
//   * Capacity is a power of two and fixed. insert() fails above 7/8 load instead of rehashing,
//     so the table never allocates after construction and its behaviour is deterministic;
//   * the value kNone (UINT32_MAX) marks an empty slot, so it cannot be stored;
//   * no iteration API: iteration order would depend on hash layout. Keep the objects in a pool
//     and walk that in handle order when determinism matters;
//   * longest_cluster() is a diagnostic (O(Capacity)) for tests that pin the probe structure.
#include "hftkit/check/cluster.hpp"
#include "hftkit/hash/fib.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace hftkit {

template <std::size_t Capacity>
class OpenIndex {
  static_assert(std::has_single_bit(Capacity) && Capacity >= 8, "Capacity: power of two >= 8");

 public:
  using Key = std::uint64_t;
  using Value = std::uint32_t;
  static constexpr Value kNone = UINT32_MAX;
  static constexpr std::size_t kMaxSize = Capacity - Capacity / 8;

  OpenIndex() : slots_(std::make_unique<Slot[]>(Capacity)) { clear(); }

  void clear() noexcept {
    for (std::size_t i = 0; i < Capacity; ++i) slots_[i] = Slot{0, kNone};
    size_ = 0;
  }

  // False if the key is present, the table is at max load, or value == kNone.
  bool insert(Key key, Value value) noexcept {
    if (value == kNone || size_ >= kMaxSize) return false;
    std::size_t i = home(key);
    for (;;) {
      Slot& s = slots_[i];
      if (s.value == kNone) {
        s = Slot{key, value};
        ++size_;
        return true;
      }
      if (s.key == key) return false;
      i = (i + 1) & kMask;
    }
  }

  [[nodiscard]] Value find(Key key) const noexcept {
    std::size_t i = home(key);
    for (;;) {
      const Slot& s = slots_[i];
      if (s.value == kNone) return kNone;
      if (s.key == key) return s.value;
      i = (i + 1) & kMask;
    }
  }

  [[nodiscard]] bool contains(Key key) const noexcept { return find(key) != kNone; }

  bool erase(Key key) noexcept {
    std::size_t hole = home(key);
    for (;;) {
      if (slots_[hole].value == kNone) return false;
      if (slots_[hole].key == key) break;
      hole = (hole + 1) & kMask;
    }
    // Backward shift: a later entry may move into the hole unless its home slot lies in the
    // cyclic range (hole, j], where moving it would put it before its own home.
    std::size_t j = hole;
    for (;;) {
      j = (j + 1) & kMask;
      if (slots_[j].value == kNone) break;
      const std::size_t h = home(slots_[j].key);
      const bool home_in_range = hole <= j ? (h > hole && h <= j) : (h > hole || h <= j);
      if (!home_in_range) {
        slots_[hole] = slots_[j];
        hole = j;
      }
    }
    slots_[hole].value = kNone;
    --size_;
    return true;
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }
  [[nodiscard]] static constexpr std::size_t max_size() noexcept { return kMaxSize; }

  [[nodiscard]] std::uint32_t longest_cluster() const noexcept {
    return hftkit::longest_cluster(Capacity,
                                   [this](std::size_t i) { return slots_[i].value != kNone; });
  }

 private:
  struct Slot {
    Key key;
    Value value;
  };
  static constexpr std::size_t kMask = Capacity - 1;
  static constexpr unsigned kShift = 64U - static_cast<unsigned>(std::countr_zero(Capacity));

  [[nodiscard]] static std::size_t home(Key key) noexcept { return fib_index(key, kShift); }

  std::unique_ptr<Slot[]> slots_;
  std::size_t size_ = 0;
};

}  // namespace hftkit
