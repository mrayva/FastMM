#pragma once
// CRC-32C (Castagnoli, reflected polynomial 0x82F63B78), table driven, constexpr table.
// crc32c("123456789") == 0xE3069283. `seed` chains calls: crc32c(b, crc32c(a)) == crc32c(a || b).
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace hftkit {

namespace detail {
[[nodiscard]] constexpr std::array<std::uint32_t, 256> make_crc32c_table() noexcept {
  std::array<std::uint32_t, 256> t{};
  for (std::uint32_t i = 0; i < 256; ++i) {
    std::uint32_t c = i;
    for (int k = 0; k < 8; ++k) c = (c & 1U) != 0 ? (c >> 1) ^ 0x82F63B78U : c >> 1;
    t[i] = c;
  }
  return t;
}
inline constexpr std::array<std::uint32_t, 256> kCrc32cTable = make_crc32c_table();
}  // namespace detail

[[nodiscard]] constexpr std::uint32_t crc32c(std::span<const std::byte> data,
                                             std::uint32_t seed = 0) noexcept {
  std::uint32_t c = ~seed;
  for (const std::byte b : data) {
    c = detail::kCrc32cTable[(c ^ std::to_integer<std::uint32_t>(b)) & 0xFFU] ^ (c >> 8);
  }
  return ~c;
}

}  // namespace hftkit
