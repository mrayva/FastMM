#pragma once
// Reorder-window sizing for sequence-numbered feeds.
//
// A reorder buffer indexed by sequence number holds `slots` messages past the next expected one.
// How much TIME that covers depends on the message rate: slots / rate. At 1M msg/s a 4096-slot
// window is ~4 ms, but at a 10M msg/s burst it is ~0.4 ms, which is shorter than a typical
// retransmit round trip. A single line can then overflow before a re-request is answered, and
// redundant A/B lines (not the buffer) are the defence. Size by time, then check it against the
// round trip, instead of picking a slot count and hoping.
#include <bit>
#include <cstddef>
#include <cstdint>

namespace hftkit {

// Seconds of feed that `slots` sequence numbers cover at `msgs_per_sec`.
[[nodiscard]] constexpr double window_seconds(std::size_t slots, double msgs_per_sec) noexcept {
  return msgs_per_sec > 0.0 ? static_cast<double>(slots) / msgs_per_sec : 0.0;
}

// Smallest power-of-two slot count covering `seconds` at `msgs_per_sec`.
[[nodiscard]] constexpr std::size_t slots_for(double msgs_per_sec, double seconds) noexcept {
  const double need = msgs_per_sec * seconds;
  if (need <= 1.0) return 1;
  auto n = static_cast<std::size_t>(need);
  if (static_cast<double>(n) < need) ++n;
  return std::bit_ceil(n);
}

// Does a window of `slots` outlast a retransmit round trip of `rtt_seconds` with `margin` (e.g. 2)?
[[nodiscard]] constexpr bool covers_round_trip(std::size_t slots,
                                               double msgs_per_sec,
                                               double rtt_seconds,
                                               double margin) noexcept {
  return window_seconds(slots, msgs_per_sec) >= rtt_seconds * margin;
}

}  // namespace hftkit
