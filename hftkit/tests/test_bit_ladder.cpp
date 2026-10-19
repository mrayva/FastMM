#include "check.hpp"
#include "hftkit/ds/bit_ladder.hpp"

#include <random>
#include <set>

using namespace hftkit;

static std::size_t model_next(const std::set<std::size_t>& s, std::size_t from) {
  const auto it = s.lower_bound(from);
  return it == s.end() ? BitLadder::npos : *it;
}
static std::size_t model_prev(const std::set<std::size_t>& s, std::size_t from) {
  auto it = s.upper_bound(from);
  return it == s.begin() ? BitLadder::npos : *std::prev(it);
}

TEST(matches_a_std_set_across_sizes_and_densities) {
  std::mt19937_64 rng(7);
  for (const std::size_t bits : {1UL, 2UL, 63UL, 64UL, 65UL, 4095UL, 4096UL, 4097UL, 70000UL}) {
    for (const int density : {2, 50, 900}) {  // few, some, most bits set
      BitLadder lad(bits);
      std::set<std::size_t> model;
      for (int step = 0; step < 6000; ++step) {
        const std::size_t i = rng() % bits;
        if (static_cast<int>(rng() % 1000) < density) {
          lad.set(i);
          model.insert(i);
        } else {
          lad.reset(i);
          model.erase(i);
        }
        CHECK_EQ(lad.test(i), model.contains(i));
        if (step % 7 == 0) {
          const std::size_t q = rng() % (bits + 3);  // includes out-of-range probes
          CHECK_EQ(lad.next_set(q), q >= bits ? BitLadder::npos : model_next(model, q));
          CHECK_EQ(lad.prev_set(q), model_prev(model, q >= bits ? bits - 1 : q));
          CHECK_EQ(lad.empty(), model.empty());
          CHECK_EQ(lad.first(), model.empty() ? BitLadder::npos : *model.begin());
          CHECK_EQ(lad.last(), model.empty() ? BitLadder::npos : *model.rbegin());
        }
      }
    }
  }
}

TEST(skips_wide_empty_gaps) {
  BitLadder lad(1U << 20);
  lad.set(5);
  lad.set((1U << 20) - 7);
  CHECK_EQ(lad.next_set(6), (1UL << 20) - 7);
  CHECK_EQ(lad.prev_set((1UL << 20) - 8), 5UL);
  lad.reset(5);
  CHECK_EQ(lad.first(), (1UL << 20) - 7);
  CHECK_EQ(lad.next_set((1UL << 20) - 6), BitLadder::npos);
}

TEST_MAIN()
