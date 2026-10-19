#include "check.hpp"
#include "hftkit/journal/ack_gate.hpp"

#include <random>
#include <vector>

using namespace hftkit;

TEST(acks_wait_for_the_durable_watermark_and_keep_their_order) {
  AckGate<8> g;
  for (std::uint64_t s = 1; s <= 5; ++s) CHECK(g.push(s, s * 10));
  g.on_journaled(5);
  std::vector<std::uint64_t> got;
  const auto take = [&](const AckGate<8>::Ack& a) { got.push_back(a.tag); };
  CHECK_EQ(g.release(take), 0U);  // journaled is not durable
  CHECK(g.on_durable(3));
  CHECK_EQ(g.release(take), 3U);
  CHECK(got == (std::vector<std::uint64_t>{10, 20, 30}));
  CHECK_EQ(g.pending(), 2U);
  CHECK(g.on_durable(5));
  CHECK_EQ(g.release(take), 2U);
  CHECK(got.back() == 50);
}

TEST(durable_can_never_pass_journaled) {
  AckGate<4> g;
  g.on_journaled(7);
  CHECK(!g.on_durable(8));
  CHECK_EQ(g.durable(), 0ULL);
  CHECK(g.on_durable(7));
  CHECK(g.on_durable(2));  // lower marks are ignored, not errors
  CHECK_EQ(g.durable(), 7ULL);
}

TEST(push_refuses_a_non_increasing_sequence_and_a_full_queue) {
  AckGate<4> g;
  CHECK(g.push(10, 0));
  CHECK(!g.push(10, 0));
  CHECK(!g.push(9, 0));
  CHECK(g.push(11, 0));
  CHECK(g.push(12, 0));
  CHECK(g.push(13, 0));
  CHECK(!g.push(14, 0));  // full
  g.on_journaled(13);
  g.on_durable(11);
  CHECK_EQ(g.release([](const AckGate<4>::Ack&) {}), 2U);
  CHECK(g.push(14, 0));  // room again, ring wrapped
}

// Property: no ack is ever handed over before the durable watermark covers it.
TEST(no_ack_is_ever_released_early) {
  AckGate<64> g;
  std::mt19937_64 rng(11);
  std::uint64_t seq = 0;
  std::uint64_t last_released = 0;
  for (int step = 0; step < 200000; ++step) {
    switch (rng() % 4) {
      case 0:
        if (g.push(seq + 1, seq + 1)) ++seq;
        break;
      case 1:
        g.on_journaled(seq > 3 ? seq - rng() % 4 : seq);
        break;
      case 2:
        g.on_durable(g.journaled() > 2 ? g.journaled() - rng() % 3 : g.journaled());
        break;
      default:
        g.release([&](const AckGate<64>::Ack& a) {
          CHECK(a.seq <= g.durable());
          CHECK(a.seq <= g.journaled());
          CHECK(a.seq > last_released);
          last_released = a.seq;
        });
    }
    CHECK(g.durable() <= g.journaled());
  }
}

TEST_MAIN()
