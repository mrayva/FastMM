#pragma once
// Fibonacci hashing and symbol -> partition routing.
//
// fib_hash() multiplies by floor(2^64 / phi). The GOOD bits of the product are the HIGH ones, so a
// table of 2^k slots takes `fib_index()` = the top k bits. Taking the low bits, or truncating to
// 32 bits first, collapses sequential keys into one long probe cluster (see check/cluster.hpp and
// tests/test_open_index.cpp, which reproduces that failure on purpose).
//
// route_partition() is a multiply-shift reduction of the same hash onto [0, partitions): no modulo,
// no floating point, identical on every platform, so independent implementations route a symbol to
// the same partition. Its reference values (partitions = 4: symbols 0..7 -> 0 2 0 3 1 0 2 1 and
// 4294967295 -> 3) are the published ones from the orderer routing spec, checked in the tests.
#include <cstddef>
#include <cstdint>

namespace hftkit {

inline constexpr std::uint64_t kFibMultiplier = 0x9E3779B97F4A7C15ULL;

[[nodiscard]] constexpr std::uint64_t fib_hash(std::uint64_t key) noexcept {
  return key * kFibMultiplier;
}

// Slot of `key` in a table of 2^(64 - shift) slots.
[[nodiscard]] constexpr std::size_t fib_index(std::uint64_t key, unsigned shift) noexcept {
  return static_cast<std::size_t>(fib_hash(key) >> shift);
}

// partitions must be in [1, 2^32).
[[nodiscard]] constexpr std::uint32_t route_partition(std::uint32_t symbol,
                                                      std::uint32_t partitions) noexcept {
  const std::uint64_t h = static_cast<std::uint64_t>(symbol) * kFibMultiplier;
  return static_cast<std::uint32_t>(((h >> 32) * partitions) >> 32);
}

}  // namespace hftkit
