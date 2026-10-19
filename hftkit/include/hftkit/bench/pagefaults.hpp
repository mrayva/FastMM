#pragma once
// Page-fault counters for "no page faults on the hot path" claims (POSIX).
//
// A structure that writes only part of its storage at construction faults the rest in later, on
// first touch, inside the code you meant to time: 516 minor faults in the first million events of
// one order book, then none. Measure it instead of asserting it:
//
//     hftkit::FaultScope faults;
//     run_hot_loop();
//     report(faults.minor());          // want 0 after warm-up
#include <sys/resource.h>

#include <cstdint>

namespace hftkit {

[[nodiscard]] inline std::uint64_t minor_faults() noexcept {
  rusage ru{};
  getrusage(RUSAGE_SELF, &ru);
  return static_cast<std::uint64_t>(ru.ru_minflt);
}

[[nodiscard]] inline std::uint64_t major_faults() noexcept {
  rusage ru{};
  getrusage(RUSAGE_SELF, &ru);
  return static_cast<std::uint64_t>(ru.ru_majflt);
}

class FaultScope {
 public:
  FaultScope() noexcept : minor0_(minor_faults()), major0_(major_faults()) {}
  [[nodiscard]] std::uint64_t minor() const noexcept { return minor_faults() - minor0_; }
  [[nodiscard]] std::uint64_t major() const noexcept { return major_faults() - major0_; }

 private:
  std::uint64_t minor0_;
  std::uint64_t major0_;
};

}  // namespace hftkit
