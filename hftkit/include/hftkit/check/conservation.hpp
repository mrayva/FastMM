#pragma once
// Quantity conservation for an order book under test.
//
// Every share submitted ends up in exactly one bucket. A fill consumes the same quantity from the
// aggressor and from the resting order, so it counts twice on the submitted side:
//
//   submitted == 2 * filled + resting + cancelled + rejected + expired
//
//   submitted  quantity of every order accepted for processing (an amend that raises an order's
//              quantity submits the increase)
//   filled     total traded quantity, counted once per trade
//   resting    quantity still on the book
//   cancelled  removed by a cancel or by an amend that lowers the quantity
//   rejected   refused whole (validation, FOK that could not fill entirely)
//   expired    the unfilled rest of an IOC or market order
//
// The identity is cheap enough to assert after EVERY event of a fuzz run. It catches lost or
// duplicated quantity (an order dropped on capacity rejection, a partial fill booked twice) that a
// fill-by-fill comparison would only notice if the reference model also had the quantity.
#include <cstdint>
#include <string>

namespace hftkit {

struct QtyLedger {
  std::int64_t submitted = 0;
  std::int64_t filled = 0;
  std::int64_t resting = 0;
  std::int64_t cancelled = 0;
  std::int64_t rejected = 0;
  std::int64_t expired = 0;

  // Zero when the books balance.
  [[nodiscard]] constexpr std::int64_t imbalance() const noexcept {
    return submitted - (2 * filled + resting + cancelled + rejected + expired);
  }
  [[nodiscard]] constexpr bool balanced() const noexcept { return imbalance() == 0; }

  [[nodiscard]] std::string describe() const {
    return "submitted=" + std::to_string(submitted) + " filled=" + std::to_string(filled) +
           " resting=" + std::to_string(resting) + " cancelled=" + std::to_string(cancelled) +
           " rejected=" + std::to_string(rejected) + " expired=" + std::to_string(expired) +
           " imbalance=" + std::to_string(imbalance());
  }
};

}  // namespace hftkit
