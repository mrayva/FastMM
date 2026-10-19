#pragma once
// BitLadder: occupancy bitmap with a summary level, for "next non-empty price level" searches.
//
// A direct-indexed price ladder keeps one bit per tick. Level 0 holds the bits; level 1 holds one
// bit per level-0 word saying "this word is not empty". next_set()/prev_set() therefore skip 64
// empty ticks per level-0 word, and 4096 empty ticks per level-1 word, instead of scanning
// tick by tick. All storage is allocated in the constructor.
//
// next_set(i): the lowest set bit >= i, or npos.   prev_set(i): the highest set bit <= i, or npos.
// The level-1 scan is linear in the number of level-1 words (bits / 4096), which is a handful for
// any realistic ladder; the constructor does not add a third level.
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace hftkit {

class BitLadder {
 public:
  static constexpr std::size_t npos = static_cast<std::size_t>(-1);

  explicit BitLadder(std::size_t bits)
      : bits_(bits), l0_((bits + 63) / 64, 0), l1_(((bits + 63) / 64 + 63) / 64, 0) {}

  [[nodiscard]] std::size_t bits() const noexcept { return bits_; }

  void set(std::size_t i) noexcept {
    const std::size_t w = i >> 6;
    l0_[w] |= std::uint64_t{1} << (i & 63);
    l1_[w >> 6] |= std::uint64_t{1} << (w & 63);
  }

  void reset(std::size_t i) noexcept {
    const std::size_t w = i >> 6;
    l0_[w] &= ~(std::uint64_t{1} << (i & 63));
    if (l0_[w] == 0) l1_[w >> 6] &= ~(std::uint64_t{1} << (w & 63));
  }

  [[nodiscard]] bool test(std::size_t i) const noexcept {
    return ((l0_[i >> 6] >> (i & 63)) & 1U) != 0;
  }

  [[nodiscard]] bool empty() const noexcept {
    for (const std::uint64_t w : l1_) {
      if (w != 0) return false;
    }
    return true;
  }

  [[nodiscard]] std::size_t next_set(std::size_t from) const noexcept {
    if (from >= bits_) return npos;
    const std::size_t w = from >> 6;
    if (const std::uint64_t m = l0_[w] & (~std::uint64_t{0} << (from & 63)); m != 0) {
      return (w << 6) + static_cast<std::size_t>(std::countr_zero(m));
    }
    const std::size_t nw = w + 1;  // first level-0 word still to look at
    if (nw >= l0_.size()) return npos;
    std::size_t sw = nw >> 6;
    std::uint64_t m1 = l1_[sw] & (~std::uint64_t{0} << (nw & 63));
    for (;;) {
      if (m1 != 0) {
        const std::size_t word = (sw << 6) + static_cast<std::size_t>(std::countr_zero(m1));
        return (word << 6) + static_cast<std::size_t>(std::countr_zero(l0_[word]));
      }
      if (++sw >= l1_.size()) return npos;
      m1 = l1_[sw];
    }
  }

  [[nodiscard]] std::size_t prev_set(std::size_t from) const noexcept {
    if (bits_ == 0) return npos;
    if (from >= bits_) from = bits_ - 1;
    const std::size_t w = from >> 6;
    if (const std::uint64_t m = l0_[w] & (~std::uint64_t{0} >> (63 - (from & 63))); m != 0) {
      return (w << 6) + 63 - static_cast<std::size_t>(std::countl_zero(m));
    }
    if (w == 0) return npos;
    const std::size_t pw = w - 1;  // last level-0 word still to look at
    std::size_t sw = pw >> 6;
    std::uint64_t m1 = l1_[sw] & (~std::uint64_t{0} >> (63 - (pw & 63)));
    for (;;) {
      if (m1 != 0) {
        const std::size_t word = (sw << 6) + 63 - static_cast<std::size_t>(std::countl_zero(m1));
        return (word << 6) + 63 - static_cast<std::size_t>(std::countl_zero(l0_[word]));
      }
      if (sw == 0) return npos;
      m1 = l1_[--sw];
    }
  }

  [[nodiscard]] std::size_t first() const noexcept { return next_set(0); }
  [[nodiscard]] std::size_t last() const noexcept { return prev_set(bits_ == 0 ? 0 : bits_ - 1); }

 private:
  std::size_t bits_;
  std::vector<std::uint64_t> l0_;
  std::vector<std::uint64_t> l1_;
};

}  // namespace hftkit
