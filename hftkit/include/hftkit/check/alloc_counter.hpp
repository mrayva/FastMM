#pragma once
// Counts calls to the global operator new, to test "no allocation on this path".
//
// Replacement allocation functions may be defined in exactly one translation unit of a program, so
// the definitions are opt-in: in ONE .cpp of the test binary, write
//
//     #define HFTKIT_ALLOC_COUNTER_DEFINE
//     #include "hftkit/check/alloc_counter.hpp"
//
// Everywhere else just include the header and use AllocScope:
//
//     hftkit::AllocScope scope;
//     hot_path();
//     CHECK(scope.count() == 0);
//
// The counter is atomic, so allocations from other threads inside the scope are counted too.
// Only operator new is counted (deletes are not), and memory is served by malloc.
#include <atomic>
#include <cstdint>

namespace hftkit {

inline std::atomic<std::uint64_t> g_new_calls{0};

[[nodiscard]] inline std::uint64_t new_calls() noexcept {
  return g_new_calls.load(std::memory_order_relaxed);
}

class AllocScope {
 public:
  AllocScope() noexcept : start_(new_calls()) {}
  // operator new calls since construction.
  [[nodiscard]] std::uint64_t count() const noexcept { return new_calls() - start_; }

 private:
  std::uint64_t start_;
};

}  // namespace hftkit

#ifdef HFTKIT_ALLOC_COUNTER_DEFINE
#include <cstdlib>
#include <new>

void* operator new(std::size_t n) {
  hftkit::g_new_calls.fetch_add(1, std::memory_order_relaxed);
  if (void* p = std::malloc(n != 0 ? n : 1)) return p;
  throw std::bad_alloc{};
}
void* operator new(std::size_t n, std::align_val_t a) {
  hftkit::g_new_calls.fetch_add(1, std::memory_order_relaxed);
  const std::size_t al = static_cast<std::size_t>(a);
  const std::size_t sz = ((n != 0 ? n : 1) + al - 1) / al * al;
  if (void* p = std::aligned_alloc(al, sz)) return p;
  throw std::bad_alloc{};
}
void operator delete(void* p) noexcept {
  std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
  std::free(p);
}
void operator delete(void* p, std::align_val_t) noexcept {
  std::free(p);
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
  std::free(p);
}
#endif
