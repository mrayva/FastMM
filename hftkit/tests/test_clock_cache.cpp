#include "check.hpp"
#include "hftkit/ds/clock_cache.hpp"

#include <random>
#include <unordered_map>
#include <vector>

using namespace hftkit;

namespace {

// A cache in front of a "slow" backing store, plus an oracle holding the latest value of every key.
struct Rig {
  static constexpr std::size_t kCap = 64;
  ClockCache<std::int64_t, kCap> cache;
  std::unordered_map<std::uint64_t, std::int64_t> store;   // what the slow store holds
  std::unordered_map<std::uint64_t, std::int64_t> oracle;  // the latest value written
  long long write_backs = 0;

  static std::int64_t initial(std::uint64_t k) { return static_cast<std::int64_t>(k) * 7 + 1; }

  std::int64_t store_get(std::uint64_t k) const {
    const auto it = store.find(k);
    return it == store.end() ? initial(k) : it->second;
  }
  std::int64_t expect(std::uint64_t k) const {
    const auto it = oracle.find(k);
    return it == oracle.end() ? initial(k) : it->second;
  }
  auto evictor() {
    return [this](std::uint64_t k, std::int64_t& v, bool dirty) {
      if (dirty) {
        store[k] = v;
        ++write_backs;
      }
    };
  }
  std::int64_t get(std::uint64_t k) {
    if (const std::int64_t* p = cache.find(k)) return *p;
    const std::int64_t v = store_get(k);
    cache.put(k, v, false, evictor());
    return v;
  }
  void set(std::uint64_t k, std::int64_t v) {
    oracle[k] = v;
    cache.put(k, v, true, evictor());
  }
  void flush() {
    cache.flush([this](std::uint64_t k, std::int64_t& v) { store[k] = v; });
  }
};

}  // namespace

TEST(reads_always_see_the_latest_write_and_flush_reaches_the_store) {
  Rig r;
  std::mt19937_64 rng(2024);
  for (int step = 0; step < 300000; ++step) {
    const std::uint64_t k = rng() % 200;  // includes key 0
    if (rng() % 100 < 60) {
      CHECK_EQ(r.get(k), r.expect(k));
    } else {
      r.set(k, static_cast<std::int64_t>(rng() % 100000));
    }
    CHECK(r.cache.size() <= Rig::kCap);
  }
  r.flush();
  for (const auto& [k, v] : r.oracle) CHECK_EQ(r.store_get(k), v);
  CHECK(r.write_backs > 0);
  CHECK_EQ(r.cache.dirty_evictions(), static_cast<std::uint64_t>(r.write_backs));
}

// A cache that finds "never used" slots by comparing the key with a default key (0) erases the
// live mapping of key 0 when it first recycles an unused slot, and loses the dirty write.
TEST(key_zero_survives_the_first_use_of_unused_slots) {
  Rig r;
  r.set(0, 42);
  for (std::uint64_t k = 1; k <= Rig::kCap + 10; ++k) r.get(k);  // fills, then recycles slots
  CHECK_EQ(r.get(0), 42);
  r.flush();
  CHECK_EQ(r.store_get(0), 42);
}

TEST(a_referenced_entry_gets_a_second_chance) {
  ClockCache<int, 4> c;
  std::vector<std::uint64_t> evicted;
  const auto ev = [&](std::uint64_t k, int&, bool) { evicted.push_back(k); };
  for (std::uint64_t k = 1; k <= 4; ++k) c.put(k, static_cast<int>(k), false, ev);  // slots 0..3
  CHECK(c.find(1) != nullptr);  // reference key 1
  c.put(
      5, 5, false, ev);  // hand at slot 0 (key 1, referenced): cleared, passed; key 2 is the victim
  CHECK_EQ(evicted.size(), 1U);
  CHECK_EQ(evicted[0], 2ULL);
  CHECK(c.peek(1) != nullptr);
  CHECK(c.peek(2) == nullptr);
  c.put(6, 6, false, ev);  // hand now at slot 2 (key 3, unreferenced)
  CHECK_EQ(evicted[1], 3ULL);
}

TEST(a_hot_working_set_that_fits_hits_almost_always) {
  Rig r;
  std::mt19937_64 rng(5);
  for (int i = 0; i < 2000; ++i) r.get(rng() % 32);  // warm
  const std::uint64_t ev0 = r.cache.evictions();
  for (int i = 0; i < 100000; ++i) r.get(rng() % 32);
  CHECK_EQ(r.cache.evictions(), ev0);  // 32 keys in 64 slots: nothing is evicted
}

TEST(invalidate_drops_without_write_back_and_frees_the_slot) {
  Rig r;
  r.set(9, 1);
  CHECK(r.cache.invalidate(9));
  CHECK(!r.cache.invalidate(9));
  CHECK(r.cache.peek(9) == nullptr);
  CHECK_EQ(r.cache.size(), 0U);
  r.flush();
  CHECK(r.store.find(9) == r.store.end());  // the dirty value was dropped on purpose
  for (std::uint64_t k = 100; k < 100 + Rig::kCap; ++k) r.set(k, 1);
  CHECK_EQ(r.cache.size(), Rig::kCap);  // the freed slot was reused, no eviction yet
  CHECK_EQ(r.cache.evictions(), 0ULL);
}

TEST_MAIN()
