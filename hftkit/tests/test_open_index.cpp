#include "check.hpp"
#include "hftkit/ds/open_index.hpp"

#include <random>
#include <unordered_map>
#include <vector>

using namespace hftkit;

TEST(matches_an_unordered_map_under_random_operations) {
  using Idx = OpenIndex<1024>;
  Idx idx;
  std::unordered_map<std::uint64_t, std::uint32_t> model;
  std::mt19937_64 rng(1234);
  for (int step = 0; step < 300000; ++step) {
    const std::uint64_t key = rng() % 1500;  // more keys than slots, so it runs at max load
    switch (rng() % 3) {
      case 0: {
        const std::uint32_t v = static_cast<std::uint32_t>(rng() % 1000000);
        const bool expect = !model.contains(key) && model.size() < Idx::max_size();
        CHECK_EQ(idx.insert(key, v), expect);
        if (expect) model[key] = v;
        break;
      }
      case 1:
        CHECK_EQ(idx.erase(key), model.erase(key) == 1);
        break;
      default: {
        const auto it = model.find(key);
        CHECK_EQ(idx.find(key), it == model.end() ? Idx::kNone : it->second);
      }
    }
    CHECK_EQ(idx.size(), model.size());
  }
  for (const auto& [k, v] : model) CHECK_EQ(idx.find(k), v);
}

TEST(kNone_cannot_be_stored_and_a_duplicate_is_refused) {
  OpenIndex<16> idx;
  CHECK(!idx.insert(5, OpenIndex<16>::kNone));
  CHECK(idx.insert(5, 1));
  CHECK(!idx.insert(5, 2));
  CHECK_EQ(idx.find(5), 1U);
}

// The structural check: sequential ids (the normal case for order references) must not cluster.
TEST(sequential_ids_do_not_form_long_probe_clusters) {
  OpenIndex<131072> idx;
  for (std::uint64_t id = 1; id <= 60000; ++id)
    CHECK(idx.insert(id, static_cast<std::uint32_t>(id)));
  CHECK(idx.longest_cluster() < 64);
  for (std::uint64_t id = 1; id <= 60000; id += 997)
    CHECK_EQ(idx.find(id), static_cast<std::uint32_t>(id));
}

// A check is only worth having if it can fail. This is the hash one order book shipped,
// (id * 2654435761) >> 32, which is about 0.618 * id: consecutive ids land in overlapping adjacent
// slots. Same table size, same linear probing, same ids; the cluster check must see the collapse.
TEST(the_cluster_check_detects_a_collapsing_hash) {
  constexpr std::size_t kSlots = 131072;
  std::vector<char> used(kSlots, 0);
  for (std::uint64_t id = 1; id <= 60000; ++id) {
    std::size_t i = static_cast<std::size_t>((id * 2654435761ULL) >> 32) & (kSlots - 1);
    while (used[i] != 0) i = (i + 1) & (kSlots - 1);
    used[i] = 1;
  }
  const std::uint32_t bad = longest_cluster(kSlots, [&](std::size_t i) { return used[i] != 0; });
  CHECK(bad > 10000);

  OpenIndex<kSlots> good;
  for (std::uint64_t id = 1; id <= 60000; ++id) good.insert(id, 1);
  CHECK(good.longest_cluster() * 100 < bad);
}

// Churn: with backward-shift deletion there are no tombstones, so clusters do not grow with age.
TEST(clusters_stay_short_under_long_churn) {
  OpenIndex<8192> idx;
  std::vector<std::uint64_t> live;
  std::uint64_t next = 1;
  std::mt19937_64 rng(99);
  while (idx.size() < 4096) {
    idx.insert(next, 1);
    live.push_back(next++);
  }
  for (int i = 0; i < 1000000; ++i) {  // a cancel and a new order, over and over
    const std::size_t at = rng() % live.size();
    CHECK(idx.erase(live[at]));
    live[at] = next;
    CHECK(idx.insert(next++, 1));
  }
  CHECK(idx.longest_cluster() < 128);
  CHECK_EQ(idx.size(), 4096U);
  for (const std::uint64_t k : live) CHECK(idx.contains(k));
}

TEST_MAIN()
