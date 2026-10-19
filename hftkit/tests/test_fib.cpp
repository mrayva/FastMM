#include "check.hpp"
#include "hftkit/hash/fib.hpp"

#include <array>

using namespace hftkit;

// Reference values published in the orderer routing spec (partitions = 4).
TEST(route_partition_matches_the_published_reference_values) {
  constexpr std::array<std::uint32_t, 8> expect = {0, 2, 0, 3, 1, 0, 2, 1};
  for (std::uint32_t s = 0; s < expect.size(); ++s) CHECK_EQ(route_partition(s, 4), expect[s]);
  CHECK_EQ(route_partition(4294967295U, 4), 3U);
}

// ... and "symbols 0..63 at P = 4 split 17/15/16/16".
TEST(route_partition_spreads_dense_ids_evenly) {
  std::array<int, 4> n{};
  for (std::uint32_t s = 0; s < 64; ++s) ++n[route_partition(s, 4)];
  CHECK_EQ(n[0], 17);
  CHECK_EQ(n[1], 15);
  CHECK_EQ(n[2], 16);
  CHECK_EQ(n[3], 16);
}

TEST(route_partition_edge_cases) {
  for (std::uint32_t s : {0U, 1U, 12345U, 4294967295U}) CHECK_EQ(route_partition(s, 1), 0U);
  for (std::uint32_t p : {2U, 3U, 7U, 1024U}) {
    for (std::uint32_t s = 0; s < 5000; ++s) CHECK(route_partition(s, p) < p);
  }
}

TEST(fib_index_stays_in_the_table) {
  for (unsigned bits : {3U, 10U, 17U, 32U}) {
    const unsigned shift = 64U - bits;
    for (std::uint64_t k = 0; k < 3000; ++k)
      CHECK(fib_index(k * 7919, shift) < (std::size_t{1} << bits));
  }
}

TEST_MAIN()
