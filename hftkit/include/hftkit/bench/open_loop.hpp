#pragma once
// Open-loop latency measurement that does not hide stalls (coordinated omission).
//
// A closed loop (do work, measure it, do the next) stops generating load while the system is
// stalled, so one 20 ms stall appears as ONE slow sample. A real client keeps sending: everything
// scheduled during the stall waits, and each of those requests is slow. run_open_loop() fixes the
// schedule up front, request i is INTENDED at t0 + i / rate, and measures completion minus the
// intended time. When work runs late, later requests start late but are still charged from their
// intended time, so a stall shows up as the hundreds of delayed requests it really caused.
//
// Waiting is a busy spin (a sleep would add its own jitter). Pin the calling thread and isolate
// the core for numbers worth publishing; the harness reports `late_starts`, the number of
// requests that could not start on time, so a saturated run is visible, not silent.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace hftkit {

struct LatencyReport {
  std::vector<std::uint64_t> sorted_ns;  // every measured sample, ascending
  std::uint64_t late_starts = 0;         // measured requests that began after their intended time
  double offered_rate_hz = 0;
  double achieved_rate_hz = 0;  // measured requests / wall time of the measured window

  [[nodiscard]] std::size_t samples() const noexcept { return sorted_ns.size(); }
  [[nodiscard]] std::uint64_t min() const noexcept {
    return sorted_ns.empty() ? 0 : sorted_ns.front();
  }
  [[nodiscard]] std::uint64_t max() const noexcept {
    return sorted_ns.empty() ? 0 : sorted_ns.back();
  }
  [[nodiscard]] double mean() const noexcept {
    if (sorted_ns.empty()) return 0;
    long double t = 0;
    for (const std::uint64_t v : sorted_ns) t += static_cast<long double>(v);
    return static_cast<double>(t / static_cast<long double>(sorted_ns.size()));
  }
  // Exact nearest-rank percentile, p in [0, 100]. Every sample is kept, nothing is bucketed.
  [[nodiscard]] std::uint64_t percentile(double p) const noexcept {
    if (sorted_ns.empty()) return 0;
    const double rank = std::ceil(p / 100.0 * static_cast<double>(sorted_ns.size()));
    const std::size_t idx = rank < 1.0 ? 0 : static_cast<std::size_t>(rank) - 1;
    return sorted_ns[std::min(idx, sorted_ns.size() - 1)];
  }
  // Number of samples above `ns`.
  [[nodiscard]] std::size_t count_above(std::uint64_t ns) const noexcept {
    return static_cast<std::size_t>(sorted_ns.end() -
                                    std::upper_bound(sorted_ns.begin(), sorted_ns.end(), ns));
  }
};

// op(i) does the work of request i (0-based, warm-up included). `warmup` requests run on the same
// schedule but are not recorded.
template <class Op>
[[nodiscard]] LatencyReport run_open_loop(double rate_hz,
                                          std::size_t warmup,
                                          std::size_t measured,
                                          Op&& op) {
  using Clock = std::chrono::steady_clock;
  const auto ns_now = [] {
    return static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch())
            .count());
  };
  LatencyReport rep;
  rep.offered_rate_hz = rate_hz;
  rep.sorted_ns.reserve(measured);
  const double period_ns = 1e9 / rate_hz;
  const std::int64_t t0 = ns_now() + 1'000'000;  // 1 ms to get into the loop
  std::int64_t first_intended = 0;
  std::int64_t last_done = 0;
  for (std::size_t i = 0; i < warmup + measured; ++i) {
    const std::int64_t intended =
        t0 + static_cast<std::int64_t>(static_cast<double>(i) * period_ns);
    std::int64_t start = ns_now();
    while (start < intended) start = ns_now();
    op(i);
    const std::int64_t done = ns_now();
    if (i >= warmup) {
      if (i == warmup) first_intended = intended;
      if (start > intended) ++rep.late_starts;
      rep.sorted_ns.push_back(static_cast<std::uint64_t>(done - intended));
      last_done = done;
    }
  }
  std::sort(rep.sorted_ns.begin(), rep.sorted_ns.end());
  if (last_done > first_intended) {
    rep.achieved_rate_hz =
        static_cast<double>(measured) * 1e9 / static_cast<double>(last_done - first_intended);
  }
  return rep;
}

}  // namespace hftkit
