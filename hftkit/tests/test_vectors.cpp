#include "check.hpp"
#include "hftkit/vectors/itch5.hpp"
#include "hftkit/vectors/moldudp64.hpp"

#include <cstring>

using namespace hftkit::vectors;
using namespace hftkit::vectors::itch5;

namespace {
template <class Arr>
void check_header(char type, const Arr& v) {
  CHECK_EQ(v[0], static_cast<std::uint8_t>(type));
  CHECK_EQ(v.size(), message_length(type));
  CHECK_EQ(be16(v.data() + kOffLocate), kLocate);
  CHECK_EQ(be16(v.data() + kOffTracking), kTracking);
  CHECK_EQ(be48(v.data() + kOffTimestamp), kTimestampNs);
}
bool text_eq(const std::uint8_t* p, const char* s, std::size_t n) {
  return std::memcmp(p, s, n) == 0;
}
}  // namespace

// These check the vectors against the field offsets of the spec tables (kept in itch5.hpp as
// constants, separately from the bytes). The tests that matter for a decoder are the ones that run
// the same bytes through the decoder; this keeps the vectors honest.
TEST(add_order_vectors_decode_at_the_spec_offsets) {
  for (const auto* v : {&kAdd}) {
    check_header('A', *v);
    CHECK_EQ(be64(v->data() + kAddOffRef), kAddExpect.ref);
    CHECK_EQ((*v)[kAddOffSide], static_cast<std::uint8_t>(kAddExpect.side));
    CHECK_EQ(be32(v->data() + kAddOffShares), kAddExpect.shares);
    CHECK(text_eq(v->data() + kAddOffStock, kAddExpect.stock, 8));
    CHECK_EQ(be32(v->data() + kAddOffPrice), kAddExpect.price4);
  }
  check_header('F', kAddMpid);
  CHECK_EQ(be64(kAddMpid.data() + kAddOffRef), kAddMpidExpect.ref);
  CHECK_EQ(kAddMpid[kAddOffSide], static_cast<std::uint8_t>(kAddMpidExpect.side));
  CHECK_EQ(be32(kAddMpid.data() + kAddOffShares), kAddMpidExpect.shares);
  CHECK(text_eq(kAddMpid.data() + kAddOffStock, kAddMpidExpect.stock, 8));
  CHECK_EQ(be32(kAddMpid.data() + kAddOffPrice), kAddMpidExpect.price4);
  CHECK(text_eq(kAddMpid.data() + kAddOffAttribution, kAddMpidAttribution, 4));
}

TEST(execution_cancel_delete_replace_vectors_decode_at_the_spec_offsets) {
  check_header('E', kExecuted);
  CHECK_EQ(be64(kExecuted.data() + kExecOffRef), kExecutedExpect.ref);
  CHECK_EQ(be32(kExecuted.data() + kExecOffShares), kExecutedExpect.shares);
  CHECK_EQ(be64(kExecuted.data() + kExecOffMatch), kExecutedExpect.match);

  check_header('C', kExecutedWithPrice);
  CHECK_EQ(be64(kExecutedWithPrice.data() + kExecPxOffRef), kExecutedWithPriceExpect.ref);
  CHECK_EQ(be32(kExecutedWithPrice.data() + kExecPxOffShares), kExecutedWithPriceExpect.shares);
  CHECK_EQ(be64(kExecutedWithPrice.data() + kExecPxOffMatch), kExecutedWithPriceExpect.match);
  CHECK_EQ(kExecutedWithPrice[kExecPxOffPrintable],
           static_cast<std::uint8_t>(kExecutedWithPriceExpect.printable));
  CHECK_EQ(be32(kExecutedWithPrice.data() + kExecPxOffPrice), kExecutedWithPriceExpect.price4);

  check_header('X', kCancel);
  CHECK_EQ(be64(kCancel.data() + kCancelOffRef), kCancelExpect.ref);
  CHECK_EQ(be32(kCancel.data() + kCancelOffShares), kCancelExpect.shares);

  check_header('D', kDelete);
  CHECK_EQ(be64(kDelete.data() + kDeleteOffRef), kDeleteExpectRef);

  check_header('U', kReplace);
  CHECK_EQ(be64(kReplace.data() + kReplaceOffOrigRef), kReplaceExpect.orig_ref);
  CHECK_EQ(be64(kReplace.data() + kReplaceOffNewRef), kReplaceExpect.new_ref);
  CHECK_EQ(be32(kReplace.data() + kReplaceOffShares), kReplaceExpect.shares);
  CHECK_EQ(be32(kReplace.data() + kReplaceOffPrice), kReplaceExpect.price4);
}

TEST(trade_vectors_decode_at_the_spec_offsets) {
  check_header('P', kTrade);
  CHECK_EQ(be64(kTrade.data() + kTradeOffRef), kTradeExpect.ref);
  CHECK_EQ(kTrade[kTradeOffSide], static_cast<std::uint8_t>(kTradeExpect.side));
  CHECK_EQ(be32(kTrade.data() + kTradeOffShares), kTradeExpect.shares);
  CHECK(text_eq(kTrade.data() + kTradeOffStock, kTradeExpect.stock, 8));
  CHECK_EQ(be32(kTrade.data() + kTradeOffPrice), kTradeExpect.price4);
  CHECK_EQ(be64(kTrade.data() + kTradeOffMatch), kTradeExpect.match);

  check_header('Q', kCross);
  CHECK_EQ(be64(kCross.data() + kCrossOffShares), kCrossExpect.shares);
  CHECK(text_eq(kCross.data() + kCrossOffStock, kCrossExpect.stock, 8));
  CHECK_EQ(be32(kCross.data() + kCrossOffPrice), kCrossExpect.price4);
  CHECK_EQ(be64(kCross.data() + kCrossOffMatch), kCrossExpect.match);
  CHECK_EQ(kCross[kCrossOffType], static_cast<std::uint8_t>(kCrossExpect.cross_type));
}

TEST(moldudp64_vectors_decode_at_the_spec_offsets) {
  using namespace moldudp64;
  CHECK_EQ(kDownstream.size(), kHeaderBytes + 2 + 19 + 2 + 23);
  CHECK(text_eq(kDownstream.data() + kOffSession, kSession, 10));
  CHECK_EQ(be64(kDownstream.data() + kOffSequence), kDownstreamSequence);
  CHECK_EQ(be16(kDownstream.data() + kOffCount), kDownstreamCount);
  std::size_t off = kOffBlocks;
  const char types[2] = {'D', 'X'};
  for (const char t : types) {  // walk the message blocks
    const std::size_t len = be16(kDownstream.data() + off);
    CHECK_EQ(len, message_length(t));
    CHECK_EQ(kDownstream[off + 2], static_cast<std::uint8_t>(t));
    off += 2 + len;
  }
  CHECK_EQ(off, kDownstream.size());

  CHECK_EQ(be16(kHeartbeat.data() + kOffCount), kHeartbeatCount);
  CHECK_EQ(be64(kHeartbeat.data() + kOffSequence), kHeartbeatSequence);
  CHECK_EQ(be16(kEndOfSession.data() + kOffCount), kEndOfSessionCount);
  CHECK_EQ(be64(kEndOfSession.data() + kOffSequence), kEndOfSessionSequence);
  CHECK_EQ(be64(kRequest.data() + kOffSequence), kRequestSequence);
  CHECK_EQ(be16(kRequest.data() + kOffCount), kRequestCount);
  CHECK(text_eq(kRequest.data() + kOffSession, kSession, 10));
}

TEST_MAIN()
