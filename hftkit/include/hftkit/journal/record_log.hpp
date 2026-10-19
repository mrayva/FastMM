#pragma once
// Checksummed record framing with crash-damage classification.
//
//   record := u32 payload_len (LE) | u32 crc32c(payload) (LE) | payload      (payload_len >= 1)
//
// scan_records() walks a buffer (a file mapped or read into memory) and says how much of it is a
// valid prefix and what lies after it. It separates the three situations a crash can leave from
// real corruption, because the right response differs:
//
//   Clean           the buffer is exactly whole valid records.
//   TornTail        the last record is incomplete, or complete but its checksum fails, and nothing
//                   valid follows it: an interrupted write. Truncate to valid_bytes.
//   ZeroFilledTail  whole records, then only zero bytes: a file extended by an interrupted write
//                   (some filesystems extend a file with zeros before the data lands). Cut them.
//   Corrupt         a checksum or header fails with valid-looking data after it: not a crash
//                   artifact. Refuse to repair; valid_bytes is where the damage starts.
//
// A torn write that is later zero-extended past the record's end (header and some payload landed,
// the rest zeros) is TornTail, not Corrupt: after the failing record there is nothing but zeros.
#include "hftkit/journal/crc32c.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace hftkit {

static_assert(std::endian::native == std::endian::little,
              "record_log assumes a little-endian host");

inline constexpr std::size_t kRecordHeaderBytes = 8;
inline constexpr std::uint32_t kMaxRecordPayload = 1U << 20;
inline constexpr std::size_t kNoRoom = static_cast<std::size_t>(-1);

enum class ScanStatus : std::uint8_t { Clean, TornTail, ZeroFilledTail, Corrupt };

struct ScanResult {
  std::size_t valid_bytes = 0;  // length of the valid prefix: a record boundary
  std::size_t records = 0;      // whole valid records in that prefix
  ScanStatus status = ScanStatus::Clean;
};

[[nodiscard]] constexpr bool repairable(const ScanResult& r) noexcept {
  return r.status != ScanStatus::Corrupt;
}

// Writes one record at buf[offset...]. Returns the offset after it, or kNoRoom if it does not fit
// or the payload is empty or larger than kMaxRecordPayload.
[[nodiscard]] inline std::size_t append_record(std::span<std::byte> buf,
                                               std::size_t offset,
                                               std::span<const std::byte> payload) noexcept {
  if (payload.empty() || payload.size() > kMaxRecordPayload) return kNoRoom;
  const std::size_t total = kRecordHeaderBytes + payload.size();
  if (offset > buf.size() || buf.size() - offset < total) return kNoRoom;
  const auto len = static_cast<std::uint32_t>(payload.size());
  const std::uint32_t crc = crc32c(payload);
  std::memcpy(buf.data() + offset, &len, 4);
  std::memcpy(buf.data() + offset + 4, &crc, 4);
  std::memcpy(buf.data() + offset + kRecordHeaderBytes, payload.data(), payload.size());
  return offset + total;
}

namespace detail {
[[nodiscard]] inline bool all_zero(std::span<const std::byte> s) noexcept {
  for (const std::byte b : s) {
    if (b != std::byte{0}) return false;
  }
  return true;
}
}  // namespace detail

[[nodiscard]] inline ScanResult scan_records(std::span<const std::byte> buf) noexcept {
  ScanResult r;
  std::size_t off = 0;
  while (off < buf.size()) {
    const std::span<const std::byte> rest = buf.subspan(off);
    if (rest.size() < kRecordHeaderBytes) {
      r.status = detail::all_zero(rest) ? ScanStatus::ZeroFilledTail : ScanStatus::TornTail;
      break;
    }
    std::uint32_t len = 0;
    std::uint32_t crc = 0;
    std::memcpy(&len, rest.data(), 4);
    std::memcpy(&crc, rest.data() + 4, 4);
    if (len == 0 && crc == 0) {  // writers never emit an empty record: this is zero fill
      r.status = detail::all_zero(rest) ? ScanStatus::ZeroFilledTail : ScanStatus::Corrupt;
      break;
    }
    if (len == 0 || len > kMaxRecordPayload) {
      r.status = ScanStatus::Corrupt;
      break;
    }
    const std::size_t total = kRecordHeaderBytes + len;
    if (total > rest.size()) {  // the record runs past the end of the buffer
      r.status = ScanStatus::TornTail;
      break;
    }
    if (crc32c(rest.subspan(kRecordHeaderBytes, len)) != crc) {
      // Damaged last record (nothing but zeros after it) is a torn write; anything else is not.
      r.status = detail::all_zero(rest.subspan(total)) ? ScanStatus::TornTail : ScanStatus::Corrupt;
      break;
    }
    off += total;
    ++r.records;
  }
  r.valid_bytes = off;
  return r;
}

}  // namespace hftkit
