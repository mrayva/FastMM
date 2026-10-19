#define HFTKIT_ALLOC_COUNTER_DEFINE
#include "check.hpp"
#include "hftkit/check/alloc_counter.hpp"
#include "hftkit/ds/bit_ladder.hpp"
#include "hftkit/ds/clock_cache.hpp"
#include "hftkit/ds/open_index.hpp"
#include "hftkit/journal/ack_gate.hpp"
#include "hftkit/journal/record_log.hpp"

#include <vector>

using namespace hftkit;

TEST(the_counter_counts_allocations) {
  AllocScope none;
  volatile int x = 0;
  for (int i = 0; i < 100; ++i) x = x + i;
  CHECK_EQ(none.count(), 0U);

  AllocScope some;
  std::vector<int>* v = new std::vector<int>();
  v->push_back(1);
  CHECK(some.count() >= 2);  // the vector object and its buffer
  delete v;
}

// The fixed-capacity types allocate in their constructors and never again.
TEST(hot_operations_of_the_fixed_capacity_types_do_not_allocate) {
  OpenIndex<4096> idx;
  ClockCache<std::int64_t, 256> cache;
  BitLadder lad(100000);
  AckGate<64> gate;
  std::vector<std::byte> buf(1 << 12);
  const std::byte payload[20] = {std::byte{1}};
  std::size_t off = 0;

  AllocScope scope;
  for (std::uint64_t k = 1; k <= 3000; ++k) idx.insert(k, static_cast<std::uint32_t>(k));
  for (std::uint64_t k = 1; k <= 3000; k += 3) idx.erase(k);
  for (std::uint64_t k = 1; k <= 3000; ++k) (void)idx.find(k);
  for (std::uint64_t k = 0; k < 5000; ++k) {
    if (cache.find(k % 700) == nullptr) {
      cache.put(k % 700,
                static_cast<std::int64_t>(k),
                (k & 1U) != 0,
                [](std::uint64_t, std::int64_t&, bool) {});
    }
  }
  for (std::size_t i = 0; i < 100000; i += 61) lad.set(i);
  for (std::size_t i = 0; i < 100000; i += 61) (void)lad.next_set(i + 1);
  for (std::uint64_t s = 1; s <= 40; ++s) gate.push(s, s);
  gate.on_journaled(40);
  gate.on_durable(40);
  gate.release([](const AckGate<64>::Ack&) {});
  while ((off = append_record(buf, off, payload)) != kNoRoom && off < buf.size() - 64) {
  }
  (void)scan_records(buf);
  CHECK_EQ(scope.count(), 0U);
}

TEST_MAIN()
