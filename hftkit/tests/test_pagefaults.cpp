#include "check.hpp"
#include "hftkit/bench/pagefaults.hpp"

#include <sys/mman.h>

#include <cstddef>
#include <cstdint>

using namespace hftkit;

namespace {
constexpr std::size_t kPage = 4096;
constexpr std::size_t kPages = 256;
void touch(volatile unsigned char* p) {
  for (std::size_t i = 0; i < kPages; ++i) p[i * kPage] = 1;
}
}  // namespace

TEST(first_touch_faults_and_a_second_touch_does_not) {
  void* m =
      mmap(nullptr, kPages * kPage, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  CHECK(m != MAP_FAILED);
  auto* p = static_cast<volatile unsigned char*>(m);
  {
    FaultScope first;
    touch(p);
    CHECK(first.minor() >= kPages / 2);  // one fault per fresh page (fewer only if pages merged)
  }
  {
    FaultScope second;
    touch(p);
    CHECK(second.minor() < kPages / 4);
    CHECK_EQ(second.major(), 0ULL);
  }
  munmap(m, kPages * kPage);
}

TEST(a_prefaulted_region_does_not_fault_when_touched) {
  void* m = mmap(nullptr,
                 kPages * kPage,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE,
                 -1,
                 0);
  CHECK(m != MAP_FAILED);
  FaultScope scope;
  touch(static_cast<volatile unsigned char*>(m));
  // ASan shadow memory adds about one fault per 8 pages, so compare against "one per page"
  CHECK(scope.minor() < kPages / 4);
  munmap(m, kPages * kPage);
}

TEST_MAIN()
