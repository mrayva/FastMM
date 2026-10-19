#pragma once
// Golden byte vectors for Nasdaq TotalView-ITCH 5.0, typed BY HAND from the specification's
// message tables, with the values each field must decode to.
//
// Why they exist: tests whose messages come from the repo's own encoder agree with the repo's own
// decoder even when both misread the spec. One feed handler shipped a decoder that omitted the
// Stock Locate and Tracking Number fields, so every offset was 4 bytes short, and passed every test
// because its fixtures were built with the same wrong layout. Bytes written out here, one field at
// a time, cannot share that mistake. Feed these (each is one message, no length prefix) to the
// decoder under test and compare against the *Expect structs.
//
// Layout (spec sections 4.1 to 4.7). Every message starts: type (1), Stock Locate (2), Tracking
// Number (2), Timestamp (6, nanoseconds since midnight), so message-specific fields start at 11.
// All numbers are big-endian. Prices are 4 implied decimals ($1.0000 = 10000).
//
// Every field of a vector has a DIFFERENT value, so a swapped or shifted field is caught.
#include <array>
#include <cstddef>
#include <cstdint>

namespace hftkit::vectors::itch5 {

// ---- spec field offsets ---------------------------------------------------------------------
inline constexpr std::size_t kOffLocate = 1;
inline constexpr std::size_t kOffTracking = 3;
inline constexpr std::size_t kOffTimestamp = 5;

// 'A' Add Order (no MPID) and 'F' Add Order with MPID attribution.
inline constexpr std::size_t kAddOffRef = 11, kAddOffSide = 19, kAddOffShares = 20,
                             kAddOffStock = 24, kAddOffPrice = 32, kAddOffAttribution = 36;
// 'E' Order Executed.
inline constexpr std::size_t kExecOffRef = 11, kExecOffShares = 19, kExecOffMatch = 23;
// 'C' Order Executed With Price.
inline constexpr std::size_t kExecPxOffRef = 11, kExecPxOffShares = 19, kExecPxOffMatch = 23,
                             kExecPxOffPrintable = 31, kExecPxOffPrice = 32;
// 'X' Order Cancel, 'D' Order Delete.
inline constexpr std::size_t kCancelOffRef = 11, kCancelOffShares = 19;
inline constexpr std::size_t kDeleteOffRef = 11;
// 'U' Order Replace.
inline constexpr std::size_t kReplaceOffOrigRef = 11, kReplaceOffNewRef = 19,
                             kReplaceOffShares = 27, kReplaceOffPrice = 31;
// 'P' Trade (non-cross).
inline constexpr std::size_t kTradeOffRef = 11, kTradeOffSide = 19, kTradeOffShares = 20,
                             kTradeOffStock = 24, kTradeOffPrice = 32, kTradeOffMatch = 36;
// 'Q' Cross Trade.
inline constexpr std::size_t kCrossOffShares = 11, kCrossOffStock = 19, kCrossOffPrice = 27,
                             kCrossOffMatch = 31, kCrossOffType = 39;

// Message length in bytes for the types above; 0 for any other type.
[[nodiscard]] constexpr std::size_t message_length(char type) noexcept {
  switch (type) {
    case 'A':
      return 36;
    case 'F':
      return 40;
    case 'E':
      return 31;
    case 'C':
      return 36;
    case 'X':
      return 23;
    case 'D':
      return 19;
    case 'U':
      return 35;
    case 'P':
      return 44;
    case 'Q':
      return 40;
    default:
      return 0;
  }
}

// ---- common header values -------------------------------------------------------------------
inline constexpr std::uint16_t kLocate = 0x1234;
inline constexpr std::uint16_t kTracking = 0x5678;
inline constexpr std::uint64_t kTimestampNs = 0x010203040506ULL;  // 6 bytes, big-endian

// ---- vectors --------------------------------------------------------------------------------
// 'A' Add Order: Buy 168,496,141 AAPL @ 117.6232, reference 0x1112131415161718.
inline constexpr std::array<std::uint8_t, 36> kAdd = {
    0x41,                                            // type 'A'
    0x12, 0x34,                                      // stock locate
    0x56, 0x78,                                      // tracking number
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,              // timestamp
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,  // order reference number
    0x42,                                            // buy/sell indicator 'B'
    0x0A, 0x0B, 0x0C, 0x0D,                          // shares
    0x41, 0x41, 0x50, 0x4C, 0x20, 0x20, 0x20, 0x20,  // stock "AAPL    "
    0x00, 0x11, 0xF2, 0xA8,                          // price
};
struct AddExpect {
  std::uint64_t ref;
  char side;
  std::uint32_t shares;
  char stock[9];
  std::uint32_t price4;
};
inline constexpr AddExpect kAddExpect{
    0x1112131415161718ULL, 'B', 0x0A0B0C0DU, "AAPL    ", 0x0011F2A8U};

// 'F' Add Order with MPID: as 'A' with attribution "MPID".
inline constexpr std::array<std::uint8_t, 40> kAddMpid = {
    0x46,                                            // type 'F'
    0x12, 0x34,                                      // stock locate
    0x56, 0x78,                                      // tracking number
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,              // timestamp
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,  // order reference number
    0x53,                                            // buy/sell indicator 'S'
    0x0A, 0x0B, 0x0C, 0x0D,                          // shares
    0x41, 0x41, 0x50, 0x4C, 0x20, 0x20, 0x20, 0x20,  // stock "AAPL    "
    0x00, 0x11, 0xF2, 0xA8,                          // price
    0x4D, 0x50, 0x49, 0x44,                          // attribution "MPID"
};
inline constexpr AddExpect kAddMpidExpect{
    0x1112131415161718ULL, 'S', 0x0A0B0C0DU, "AAPL    ", 0x0011F2A8U};
inline constexpr char kAddMpidAttribution[5] = "MPID";

// 'E' Order Executed: 1,000 shares of reference 0x2122232425262728, match 0x3132333435363738.
inline constexpr std::array<std::uint8_t, 31> kExecuted = {
    0x45,                                            // type 'E'
    0x12, 0x34,                                      // stock locate
    0x56, 0x78,                                      // tracking number
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,              // timestamp
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,  // order reference number
    0x00, 0x00, 0x03, 0xE8,                          // executed shares (1000)
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,  // match number
};
struct ExecutedExpect {
  std::uint64_t ref;
  std::uint32_t shares;
  std::uint64_t match;
};
inline constexpr ExecutedExpect kExecutedExpect{
    0x2122232425262728ULL, 1000U, 0x3132333435363738ULL};

// 'C' Order Executed With Price: 100 shares at 0x00123456, printable 'Y'.
inline constexpr std::array<std::uint8_t, 36> kExecutedWithPrice = {
    0x43,                                            // type 'C'
    0x12, 0x34,                                      // stock locate
    0x56, 0x78,                                      // tracking number
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,              // timestamp
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,  // order reference number
    0x00, 0x00, 0x00, 0x64,                          // executed shares (100)
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,  // match number
    0x59,                                            // printable 'Y'
    0x00, 0x12, 0x34, 0x56,                          // execution price
};
struct ExecutedWithPriceExpect {
  std::uint64_t ref;
  std::uint32_t shares;
  std::uint64_t match;
  char printable;
  std::uint32_t price4;
};
inline constexpr ExecutedWithPriceExpect kExecutedWithPriceExpect{
    0x2122232425262728ULL, 100U, 0x4142434445464748ULL, 'Y', 0x00123456U};

// 'X' Order Cancel: 25 shares cancelled from reference 0x3132333435363738.
inline constexpr std::array<std::uint8_t, 23> kCancel = {
    0x58,                                            // type 'X'
    0x12, 0x34,                                      // stock locate
    0x56, 0x78,                                      // tracking number
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,              // timestamp
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,  // order reference number
    0x00, 0x00, 0x00, 0x19,                          // cancelled shares (25)
};
struct CancelExpect {
  std::uint64_t ref;
  std::uint32_t shares;
};
inline constexpr CancelExpect kCancelExpect{0x3132333435363738ULL, 25U};

// 'D' Order Delete: reference 0x4142434445464748.
inline constexpr std::array<std::uint8_t, 19> kDelete = {
    0x44,                                            // type 'D'
    0x12, 0x34,                                      // stock locate
    0x56, 0x78,                                      // tracking number
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,              // timestamp
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,  // order reference number
};
inline constexpr std::uint64_t kDeleteExpectRef = 0x4142434445464748ULL;

// 'U' Order Replace: 0x5152535455565758 -> 0x6162636465666768, 500 shares at 0x00ABCDEF.
inline constexpr std::array<std::uint8_t, 35> kReplace = {
    0x55,                                            // type 'U'
    0x12, 0x34,                                      // stock locate
    0x56, 0x78,                                      // tracking number
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,              // timestamp
    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,  // original order reference number
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,  // new order reference number
    0x00, 0x00, 0x01, 0xF4,                          // shares (500)
    0x00, 0xAB, 0xCD, 0xEF,                          // price
};
struct ReplaceExpect {
  std::uint64_t orig_ref;
  std::uint64_t new_ref;
  std::uint32_t shares;
  std::uint32_t price4;
};
inline constexpr ReplaceExpect kReplaceExpect{
    0x5152535455565758ULL, 0x6162636465666768ULL, 500U, 0x00ABCDEFU};

// 'P' Trade (non-cross): sell, 10 MSFT at 123,456 (12.3456).
inline constexpr std::array<std::uint8_t, 44> kTrade = {
    0x50,                                            // type 'P'
    0x12, 0x34,                                      // stock locate
    0x56, 0x78,                                      // tracking number
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,              // timestamp
    0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,  // order reference number
    0x53,                                            // buy/sell indicator 'S'
    0x00, 0x00, 0x00, 0x0A,                          // shares (10)
    0x4D, 0x53, 0x46, 0x54, 0x20, 0x20, 0x20, 0x20,  // stock "MSFT    "
    0x00, 0x01, 0xE2, 0x40,                          // price (123456)
    0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,  // match number
};
struct TradeExpect {
  std::uint64_t ref;
  char side;
  std::uint32_t shares;
  char stock[9];
  std::uint32_t price4;
  std::uint64_t match;
};
inline constexpr TradeExpect kTradeExpect{
    0x7172737475767778ULL, 'S', 10U, "MSFT    ", 123456U, 0x8182838485868788ULL};

// 'Q' Cross Trade: 50,000 GOOG at 0x000A0B0C, opening cross 'O'.
inline constexpr std::array<std::uint8_t, 40> kCross = {
    0x51,                                            // type 'Q'
    0x12, 0x34,                                      // stock locate
    0x56, 0x78,                                      // tracking number
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,              // timestamp
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3, 0x50,  // shares (8 bytes, 50000)
    0x47, 0x4F, 0x4F, 0x47, 0x20, 0x20, 0x20, 0x20,  // stock "GOOG    "
    0x00, 0x0A, 0x0B, 0x0C,                          // cross price
    0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,  // match number
    0x4F,                                            // cross type 'O'
};
struct CrossExpect {
  std::uint64_t shares;
  char stock[9];
  std::uint32_t price4;
  std::uint64_t match;
  char cross_type;
};
inline constexpr CrossExpect kCrossExpect{
    50000ULL, "GOOG    ", 0x000A0B0CU, 0x9192939495969798ULL, 'O'};

// ---- big-endian readers for tests -----------------------------------------------------------
[[nodiscard]] constexpr std::uint16_t be16(const std::uint8_t* p) noexcept {
  return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}
[[nodiscard]] constexpr std::uint32_t be32(const std::uint8_t* p) noexcept {
  return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
         (static_cast<std::uint32_t>(p[2]) << 8) | p[3];
}
[[nodiscard]] constexpr std::uint64_t be48(const std::uint8_t* p) noexcept {
  return (static_cast<std::uint64_t>(be16(p)) << 32) | be32(p + 2);
}
[[nodiscard]] constexpr std::uint64_t be64(const std::uint8_t* p) noexcept {
  return (static_cast<std::uint64_t>(be32(p)) << 32) | be32(p + 4);
}

}  // namespace hftkit::vectors::itch5
