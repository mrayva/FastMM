#include "check.hpp"
#include "hftkit/net/sizing.hpp"

#include <cmath>

using namespace hftkit;

TEST(window_time_depends_on_the_rate) {
  CHECK(std::fabs(window_seconds(4096, 1e6) - 0.004096) < 1e-12);
  CHECK(std::fabs(window_seconds(4096, 1e7) - 0.0004096) < 1e-12);
  CHECK_EQ(window_seconds(4096, 0), 0.0);
}

TEST(slots_for_rounds_up_to_a_power_of_two) {
  CHECK_EQ(slots_for(1e6, 0.004), 4096U);   // 4000 messages
  CHECK_EQ(slots_for(1e7, 0.001), 16384U);  // 10000 messages
  CHECK_EQ(slots_for(1000, 1.0), 1024U);    // exactly 1000
  CHECK_EQ(slots_for(1024, 1.0), 1024U);    // exact power of two stays
  CHECK_EQ(slots_for(1, 0.5), 1U);
}

TEST(a_window_that_covers_the_burst_round_trip) {
  // The 4096-slot window of a typical receiver: fine at 1M msg/s, too short at a 10M msg/s burst
  // for a 500 microsecond retransmit round trip.
  CHECK(covers_round_trip(4096, 1e6, 0.0005, 1.0));
  CHECK(!covers_round_trip(4096, 1e7, 0.0005, 1.0));
  CHECK(covers_round_trip(slots_for(1e7, 0.0005 * 2), 1e7, 0.0005, 2.0));
}

TEST_MAIN()
