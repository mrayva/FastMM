#include "check.hpp"
#include "hftkit/journal/record_log.hpp"

#include <cstring>
#include <string_view>
#include <vector>

using namespace hftkit;

namespace {

std::vector<std::byte> payload_of(std::size_t n, std::size_t salt) {
  std::vector<std::byte> p(n);
  for (std::size_t i = 0; i < n; ++i)
    p[i] = static_cast<std::byte>((i + salt) % 250 + 1);  // never 0
  return p;
}

struct Log {
  std::vector<std::byte> bytes;
  std::vector<std::size_t> boundaries{0};  // offsets after each whole record, starting with 0
  Log() {
    bytes.resize(1 << 14);
    std::size_t off = 0;
    std::size_t salt = 0;
    for (const std::size_t n : {1UL, 5UL, 8UL, 100UL, 3UL, 64UL, 1000UL, 7UL}) {
      const auto p = payload_of(n, salt++);
      off = append_record(bytes, off, p);
      boundaries.push_back(off);
    }
    bytes.resize(off);
  }
  std::size_t last_boundary_at_or_below(std::size_t cut) const {
    std::size_t best = 0;
    for (const std::size_t b : boundaries) {
      if (b <= cut) best = b;
    }
    return best;
  }
  std::size_t records_before(std::size_t boundary) const {
    std::size_t n = 0;
    for (const std::size_t b : boundaries) {
      if (b != 0 && b <= boundary) ++n;
    }
    return n;
  }
};

std::span<const std::byte> view(const std::vector<std::byte>& v, std::size_t n) {
  return std::span<const std::byte>(v.data(), n);
}

}  // namespace

TEST(crc32c_known_answer_and_chaining) {
  const std::string_view s = "123456789";
  const auto* p = reinterpret_cast<const std::byte*>(s.data());
  CHECK_EQ(crc32c({p, s.size()}), 0xE3069283U);
  CHECK_EQ(crc32c({p, 4}, 0) == 0, false);
  CHECK_EQ(crc32c({p + 4, 5}, crc32c({p, 4})), 0xE3069283U);
  CHECK_EQ(crc32c({}), 0U);
}

TEST(a_clean_log_scans_clean) {
  const Log log;
  const ScanResult r = scan_records(log.bytes);
  CHECK(r.status == ScanStatus::Clean);
  CHECK_EQ(r.valid_bytes, log.bytes.size());
  CHECK_EQ(r.records, log.boundaries.size() - 1);
  const ScanResult empty = scan_records({});
  CHECK(empty.status == ScanStatus::Clean);
  CHECK_EQ(empty.valid_bytes, 0U);
}

TEST(a_log_cut_at_every_byte_keeps_exactly_its_whole_records) {
  const Log log;
  for (std::size_t cut = 0; cut <= log.bytes.size(); ++cut) {
    const ScanResult r = scan_records(view(log.bytes, cut));
    const std::size_t want = log.last_boundary_at_or_below(cut);
    CHECK_EQ(r.valid_bytes, want);
    CHECK_EQ(r.records, log.records_before(want));
    if (cut == want) {
      CHECK(r.status == ScanStatus::Clean);
    } else {  // a partial header of leading zero bytes reads as zero fill; both are repairable
      CHECK(r.status == ScanStatus::TornTail || r.status == ScanStatus::ZeroFilledTail);
    }
    CHECK(repairable(r));
  }
}

TEST(zero_bytes_after_whole_records_are_a_zero_filled_tail) {
  const Log log;
  for (std::size_t zeros = 1; zeros <= 80; ++zeros) {
    std::vector<std::byte> b = log.bytes;
    b.resize(b.size() + zeros, std::byte{0});
    const ScanResult r = scan_records(b);
    CHECK(r.status == ScanStatus::ZeroFilledTail);
    CHECK_EQ(r.valid_bytes, log.bytes.size());
    CHECK_EQ(r.records, log.boundaries.size() - 1);
  }
}

// An interrupted write that the filesystem then zero-extends: the record's header and part of its
// payload landed, the rest of the file is zeros, past the record's own end. Not corruption.
TEST(a_torn_record_that_was_zero_extended_is_a_torn_tail) {
  const Log log;
  for (std::size_t k = 1; k + 1 < log.boundaries.size(); ++k) {
    const std::size_t start = log.boundaries[k];
    const std::size_t cut = start + kRecordHeaderBytes + 1;  // header + 1 payload byte landed
    if (cut >= log.boundaries[k + 1]) continue;              // record has no payload to tear
    std::vector<std::byte> b(log.bytes.begin(), log.bytes.begin() + static_cast<long>(cut));
    b.resize(log.boundaries[k + 1] + 20, std::byte{0});
    const ScanResult r = scan_records(b);
    CHECK(r.status == ScanStatus::TornTail);
    CHECK_EQ(r.valid_bytes, start);
    CHECK_EQ(r.records, k);
  }
}

TEST(damage_before_valid_data_is_corrupt_and_damage_to_the_last_record_is_a_torn_tail) {
  const Log log;
  {  // flip a payload bit of record 3 (not the last): valid records follow it
    std::vector<std::byte> b = log.bytes;
    b[log.boundaries[2] + kRecordHeaderBytes] ^= std::byte{0x01};
    const ScanResult r = scan_records(b);
    CHECK(r.status == ScanStatus::Corrupt);
    CHECK_EQ(r.valid_bytes, log.boundaries[2]);
    CHECK_EQ(r.records, 2U);
    CHECK(!repairable(r));
  }
  {  // flip a bit of the LAST record: nothing valid follows, so it is a torn write
    std::vector<std::byte> b = log.bytes;
    const std::size_t last = log.boundaries[log.boundaries.size() - 2];
    b[last + kRecordHeaderBytes] ^= std::byte{0x01};
    const ScanResult r = scan_records(b);
    CHECK(r.status == ScanStatus::TornTail);
    CHECK_EQ(r.valid_bytes, last);
  }
  {  // an impossible length in a middle record's header
    std::vector<std::byte> b = log.bytes;
    b[log.boundaries[3] + 3] = std::byte{0x7F};
    const ScanResult r = scan_records(b);
    CHECK(r.status == ScanStatus::Corrupt);
    CHECK_EQ(r.valid_bytes, log.boundaries[3]);
  }
  {  // zeros, then data: not a tail of zeros
    std::vector<std::byte> b = log.bytes;
    b.resize(b.size() + 16, std::byte{0});
    b.push_back(std::byte{1});
    CHECK(scan_records(b).status == ScanStatus::Corrupt);
  }
}

TEST(append_refuses_what_cannot_be_framed) {
  std::vector<std::byte> buf(32);
  const auto p = payload_of(10, 0);
  CHECK_EQ(append_record(buf, 0, {}), kNoRoom);                 // empty payload
  CHECK_EQ(append_record(buf, 0, payload_of(25, 0)), kNoRoom);  // 8 + 25 > 32
  CHECK_EQ(append_record(buf, 30, p), kNoRoom);                 // no room at that offset
  CHECK_EQ(append_record(buf, 100, p), kNoRoom);                // offset past the end
  CHECK_EQ(append_record(buf, 0, p), kRecordHeaderBytes + 10);
  CHECK_EQ(append_record(buf, 18, payload_of(6, 0)), 32U);  // exactly fits
}

TEST_MAIN()
