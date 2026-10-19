#pragma once
// A deliberately tiny test harness: no dependencies, a failed CHECK is reported and the test goes
// on, and the process exit code says whether anything failed. TEST(name) registers a test and
// TEST_MAIN() at the end of the file runs them.
#include <cstdio>
#include <vector>

namespace hftkit_test {

struct Case {
  const char* name;
  void (*fn)();
};
inline std::vector<Case>& cases() {
  static std::vector<Case> v;
  return v;
}
inline long long& failures() {
  static long long n = 0;
  return n;
}
inline long long& checks() {
  static long long n = 0;
  return n;
}
struct Registrar {
  Registrar(const char* name, void (*fn)()) { cases().push_back({name, fn}); }
};

inline int run_all() {
  for (const Case& c : cases()) {
    const long long before = failures();
    c.fn();
    std::printf("%-58s %s\n", c.name, failures() == before ? "ok" : "FAILED");
  }
  std::printf("%zu tests, %lld checks, %lld failed\n", cases().size(), checks(), failures());
  return failures() == 0 ? 0 : 1;
}

}  // namespace hftkit_test

#define TEST(name)                                                   \
  static void name();                                                \
  static const hftkit_test::Registrar registrar_##name(#name, name); \
  static void name()

#define CHECK(cond)                                                                 \
  do {                                                                              \
    ++hftkit_test::checks();                                                        \
    if (!(cond)) {                                                                  \
      ++hftkit_test::failures();                                                    \
      std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
    }                                                                               \
  } while (0)

#define CHECK_EQ(a, b)                                               \
  do {                                                               \
    const auto va_ = (a);                                            \
    const auto vb_ = (b);                                            \
    ++hftkit_test::checks();                                         \
    if (!(va_ == vb_)) {                                             \
      ++hftkit_test::failures();                                     \
      std::fprintf(stderr,                                           \
                   "%s:%d: CHECK_EQ(%s, %s) failed: %lld != %lld\n", \
                   __FILE__,                                         \
                   __LINE__,                                         \
                   #a,                                               \
                   #b,                                               \
                   static_cast<long long>(va_),                      \
                   static_cast<long long>(vb_));                     \
    }                                                                \
  } while (0)

#define TEST_MAIN()                \
  int main() {                     \
    return hftkit_test::run_all(); \
  }
