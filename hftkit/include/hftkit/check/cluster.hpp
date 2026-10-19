#pragma once
// Longest run of occupied slots in a cyclic open-addressing table.
//
// With linear probing the cost of a lookup or an erase is bounded by the cluster it lands in, so a
// test that asserts the longest cluster stays small checks the STRUCTURE of a table. A timing
// assertion is a poor substitute: a hash that collapses sequential ids can still pass a timing
// check on a small or warm table, while the cluster length shows the collapse directly.
#include <cstddef>
#include <cstdint>

namespace hftkit {

// `occupied(i)` says whether slot i of `n` is in use. Returns n if every slot is occupied.
template <class Occupied>
[[nodiscard]] std::uint32_t longest_cluster(std::size_t n, Occupied&& occupied) {
  std::size_t start = 0;
  while (start < n && occupied(start)) ++start;
  if (start == n) return static_cast<std::uint32_t>(n);  // no empty slot to anchor the scan
  std::size_t best = 0;
  std::size_t run = 0;
  for (std::size_t k = 1; k <= n; ++k) {
    const std::size_t i = (start + k) % n;
    if (occupied(i)) {
      ++run;
      if (run > best) best = run;
    } else {
      run = 0;
    }
  }
  return static_cast<std::uint32_t>(best);
}

}  // namespace hftkit
