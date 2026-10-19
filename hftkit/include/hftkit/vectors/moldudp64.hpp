#pragma once
// Golden byte vectors for MoldUDP64 1.00, typed by hand from the specification (see itch5.hpp for
// why). All numbers are big-endian.
//
//   Downstream packet   Session (10, alphanumeric) | Sequence Number (8) | Message Count (2) |
//                       Message Blocks...
//   Message Block       Message Length (2, excludes itself) | Message Data
//   Heartbeat           Message Count == 0, Sequence Number = next sequence to be sent
//   End of Session      Message Count == 0xFFFF, Sequence Number = next sequence
//   Request Packet      Session (10) | Sequence Number (8) | Requested Message Count (2)
#include <array>
#include <cstddef>
#include <cstdint>

namespace hftkit::vectors::moldudp64 {

inline constexpr std::size_t kHeaderBytes = 20;
inline constexpr std::size_t kOffSession = 0, kOffSequence = 10, kOffCount = 18, kOffBlocks = 20;
inline constexpr std::uint16_t kHeartbeatCount = 0;
inline constexpr std::uint16_t kEndOfSessionCount = 0xFFFF;
inline constexpr char kSession[11] = "TESTSESS01";

// Sequence 0x0102030405060708, two messages: an ITCH 'D' (19 bytes) then an ITCH 'X' (23 bytes).
inline constexpr std::array<std::uint8_t, 66> kDownstream = {
    0x54, 0x45, 0x53, 0x54, 0x53, 0x45, 0x53, 0x53, 0x30, 0x31,  // session "TESTSESS01"
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,              // sequence number of 1st message
    0x00, 0x02,                                                  // message count
    0x00, 0x13,                                                  // block 1 length (19)
    0x44, 0x12, 0x34, 0x56, 0x78, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // 'D' header
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,                    // 'D' order reference
    0x00, 0x17,                                                        // block 2 length (23)
    0x58, 0x12, 0x34, 0x56, 0x78, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // 'X' header
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,                    // 'X' order reference
    0x00, 0x00, 0x00, 0x19,                                            // 'X' cancelled shares (25)
};
inline constexpr std::uint64_t kDownstreamSequence = 0x0102030405060708ULL;
inline constexpr std::uint16_t kDownstreamCount = 2;

inline constexpr std::array<std::uint8_t, 20> kHeartbeat = {
    0x54, 0x45, 0x53, 0x54, 0x53, 0x45, 0x53, 0x53, 0x30, 0x31,  // session
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x0B,              // next sequence 0x0A0B
    0x00, 0x00,                                                  // count 0
};
inline constexpr std::uint64_t kHeartbeatSequence = 0x0A0BULL;

inline constexpr std::array<std::uint8_t, 20> kEndOfSession = {
    0x54, 0x45, 0x53, 0x54, 0x53, 0x45, 0x53, 0x53, 0x30, 0x31,  // session
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x0C,              // next sequence 0x0A0C
    0xFF, 0xFF,                                                  // end of session
};
inline constexpr std::uint64_t kEndOfSessionSequence = 0x0A0CULL;

inline constexpr std::array<std::uint8_t, 20> kRequest = {
    0x54, 0x45, 0x53, 0x54, 0x53, 0x45, 0x53, 0x53, 0x30, 0x31,  // session
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xE8,              // first requested sequence 1000
    0x00, 0x64,                                                  // requested count (100)
};
inline constexpr std::uint64_t kRequestSequence = 1000;
inline constexpr std::uint16_t kRequestCount = 100;

}  // namespace hftkit::vectors::moldudp64
