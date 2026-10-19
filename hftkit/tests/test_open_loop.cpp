#include "check.hpp"
#include "hftkit/bench/open_loop.hpp"

#include <chrono>

using namespace hftkit;
using Clock = std::chrono::steady_clock;

namespace {
void spin_for(std::chrono::microseconds d) {
  const auto end = Clock::now() + d;
  while (Clock::now() < end) {
  }
}
}  // namespace

// A 20 ms stall in the middle of a 10,000 requests/s run. Measured per call (closed loop) it is
// ONE slow sample; measured from the intended send time it is every request that was due while the
// system was stuck: about 200.
TEST(a_stall_shows_up_as_every_request_it_delayed) {
  std::size_t naive_slow = 0;
  const LatencyReport rep = run_open_loop(10000.0, 100, 2000, [&](std::size_t i) {
    const auto t0 = Clock::now();
    if (i == 400) spin_for(std::chrono::microseconds(20000));
    if (Clock::now() - t0 > std::chrono::milliseconds(1)) ++naive_slow;
  });
  CHECK_EQ(rep.samples(), 2000U);
  CHECK(naive_slow >= 1 && naive_slow <= 5);  // the closed-loop view: one stall
  CHECK(rep.count_above(1'000'000) >= 100);   // the open-loop view: the requests it delayed
  CHECK(rep.max() >= 15'000'000);
  CHECK(rep.late_starts >= 100);
}

TEST(report_statistics_are_exact_and_ordered) {
  const LatencyReport rep = run_open_loop(20000.0, 10, 500, [](std::size_t) {});
  CHECK_EQ(rep.samples(), 500U);
  for (std::size_t i = 1; i < rep.sorted_ns.size(); ++i)
    CHECK(rep.sorted_ns[i - 1] <= rep.sorted_ns[i]);
  CHECK_EQ(rep.percentile(0), rep.min());
  CHECK_EQ(rep.percentile(100), rep.max());
  CHECK(rep.percentile(50) <= rep.percentile(99));
  CHECK(rep.percentile(99) <= rep.percentile(99.9));
  CHECK(rep.mean() >= static_cast<double>(rep.min()));
  CHECK(rep.achieved_rate_hz > 0);

  LatencyReport fixed;
  fixed.sorted_ns = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
  CHECK_EQ(fixed.percentile(50), 50ULL);  // nearest rank: ceil(0.5 * 10) = 5th
  CHECK_EQ(fixed.percentile(90), 90ULL);
  CHECK_EQ(fixed.percentile(91), 100ULL);
  CHECK_EQ(fixed.count_above(60), 4U);
}

TEST_MAIN()
