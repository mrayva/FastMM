#include "check.hpp"
#include "hftkit/check/ref_book.hpp"

#include <random>
#include <vector>

using namespace hftkit;

namespace {
RefOrder lim(std::uint64_t id, RefSide s, std::int64_t px, std::int64_t q, RefTif t = RefTif::Gtc) {
  return RefOrder{id, s, RefKind::Limit, t, px, q};
}
RefOrder mkt(std::uint64_t id, RefSide s, std::int64_t q) {
  return RefOrder{id, s, RefKind::Market, RefTif::Ioc, 0, q};
}
}  // namespace

TEST(price_time_priority_and_fills_print_at_the_resting_price) {
  RefBook b;
  b.submit(lim(1, RefSide::Sell, 10, 100));
  b.submit(lim(2, RefSide::Sell, 10, 50));
  b.submit(lim(3, RefSide::Sell, 11, 30));
  const auto f = b.submit(lim(4, RefSide::Buy, 11, 120));
  CHECK_EQ(f.size(), 2U);
  CHECK((f[0] == RefFill{4, 1, 10, 100}));  // earlier order first, at its own price
  CHECK((f[1] == RefFill{4, 2, 10, 20}));
  CHECK_EQ(b.depth_at(RefSide::Sell, 10), 30);
  CHECK_EQ(b.depth_at(RefSide::Sell, 11), 30);
  CHECK(b.ledger().balanced());
  // a more aggressive limit still prints at the passive price
  const auto g = b.submit(lim(5, RefSide::Buy, 15, 10));
  CHECK((g[0] == RefFill{5, 2, 10, 10}));
}

TEST(ioc_and_market_never_rest_and_fok_is_all_or_nothing) {
  RefBook b;
  b.submit(lim(1, RefSide::Sell, 10, 40));
  b.submit(lim(2, RefSide::Sell, 11, 40));
  CHECK_EQ(b.submit(lim(3, RefSide::Buy, 10, 100, RefTif::Ioc)).size(), 1U);
  CHECK_EQ(b.live_orders(), 1U);  // IOC remainder expired; only order 2 is left
  CHECK_EQ(b.ledger().expired, 60);
  // FOK for more than is available: nothing happens at all
  const auto before = b.depth_at(RefSide::Sell, 11);
  CHECK(b.submit(lim(4, RefSide::Buy, 11, 41, RefTif::Fok)).empty());
  CHECK_EQ(b.depth_at(RefSide::Sell, 11), before);
  CHECK_EQ(b.ledger().rejected, 41);
  // FOK that can fill, across two levels
  b.submit(lim(5, RefSide::Sell, 10, 5));
  const auto f = b.submit(lim(6, RefSide::Buy, 11, 45, RefTif::Fok));
  CHECK_EQ(f.size(), 2U);
  CHECK(!b.crossed());
  // market order sweeps then expires
  b.submit(lim(7, RefSide::Sell, 12, 10));
  CHECK_EQ(b.submit(mkt(8, RefSide::Buy, 25)).size(), 1U);  // 10 available, 15 expire
  CHECK_EQ(b.live_orders(), 0U);
  CHECK(b.ledger().balanced());
}

TEST(modify_down_keeps_priority_modify_up_loses_it) {
  RefBook b;
  b.submit(lim(1, RefSide::Sell, 10, 100));
  b.submit(lim(2, RefSide::Sell, 10, 100));
  CHECK(b.modify(1, 60));  // decrease: still first
  auto f = b.submit(lim(3, RefSide::Buy, 10, 10));
  CHECK_EQ(f[0].passive, 1ULL);
  CHECK(b.modify(1, 500));  // increase: goes to the back
  f = b.submit(lim(4, RefSide::Buy, 10, 10));
  CHECK_EQ(f[0].passive, 2ULL);
  CHECK(b.modify(2, 0));  // zero is a cancel
  CHECK(!b.modify(2, 5));
  CHECK(b.ledger().balanced());
}

TEST(duplicate_live_ids_are_rejected_and_ids_can_be_reused) {
  RefBook b;
  b.submit(lim(1, RefSide::Buy, 9, 10));
  CHECK(b.submit(lim(1, RefSide::Buy, 9, 10)).empty());  // duplicate while live
  CHECK_EQ(b.live_orders(), 1U);
  CHECK_EQ(b.ledger().rejected, 10);
  b.submit(lim(2, RefSide::Sell, 9, 10));  // fills order 1 completely
  CHECK_EQ(b.live_orders(), 0U);
  b.submit(lim(1, RefSide::Buy, 8, 3));  // id 1 is free again
  CHECK_EQ(b.live_orders(), 1U);
  CHECK(b.ledger().balanced());
}

// 200,000 random events: the identity holds after every one, and the book never crosses.
TEST(the_ledger_balances_after_every_event_of_a_random_run) {
  RefBook b;
  std::mt19937_64 rng(31337);
  std::vector<std::uint64_t> ids;
  std::uint64_t next = 1;
  for (int step = 0; step < 200000; ++step) {
    const int roll = static_cast<int>(rng() % 100);
    if (roll < 55 || ids.empty()) {
      const RefSide s = (rng() & 1U) != 0 ? RefSide::Buy : RefSide::Sell;
      const int t = static_cast<int>(rng() % 10);
      const RefTif tif = t < 7 ? RefTif::Gtc : (t < 9 ? RefTif::Ioc : RefTif::Fok);
      const std::int64_t px = 95 + static_cast<std::int64_t>(rng() % 11);
      b.submit(lim(next, s, px, 1 + static_cast<std::int64_t>(rng() % 50), tif));
      ids.push_back(next++);
    } else if (roll < 63) {
      b.submit(mkt(next,
                   (rng() & 1U) != 0 ? RefSide::Buy : RefSide::Sell,
                   1 + static_cast<std::int64_t>(rng() % 50)));
      ids.push_back(next++);
    } else if (roll < 85) {
      b.cancel(ids[rng() % ids.size()]);
    } else {
      b.modify(ids[rng() % ids.size()], static_cast<std::int64_t>(rng() % 60));
    }
    CHECK(b.ledger().balanced());
    CHECK(!b.crossed());
    if (!b.ledger().balanced()) break;
  }
  CHECK(b.ledger().filled > 0);
  CHECK(b.ledger().cancelled > 0);
  CHECK(b.ledger().expired > 0);
  CHECK(b.ledger().rejected > 0);
}

TEST_MAIN()
