# 3. Feed handling

### Streaming ITCH parser with split-message cache
- **Where:** CppTrader `ITCHHandler::Process` (MIT).
- **What:** accepts arbitrary chunks, keeps a partial 2-byte length prefix and partial message across calls, dispatches by type byte to virtual handlers.
- **Evidence:** `verified` here: on `sample.itch` it costs 10–11 ns/message with the file in RAM and 15–24 ns with an 8 KB `fread` (published 24 ns on an i7-4790K).
- **Verdict:** FastMM's decoder does a different job (below); use CppTrader's as an independent oracle.

### Skip-by-length decoder with event output
- **Where:** FastMM `ItchDecoder::decode_into`.
- **Evidence:** `verified`: 4.3 ns/message over all messages, 8.3 ns per emitted event; the 77% of the sample that is NOII traffic is validated and skipped. Touching each message is a 2.3 ns floor.
- **Caveat:** not the same work as CppTrader (struct per message, virtual dispatch), so the ratio is not a like-for-like speed claim.

### Hand-typed golden vectors
- **Where:** udp-multicast-receiver (no licence, concept only); hftkit `vectors/`.
- **What:** that repo's decoder omitted the stock-locate and tracking-number fields, every Add Order field was mis-positioned, and all 202 assertions passed because fixtures were built with the same wrong layout. Hand-typed spec bytes then failed the old decoder (11 of 61 checks).
- **Evidence:** hftkit vectors `verified` against the spec offsets; the Add vector `verified` through FastMM's `ItchL2Bridge` (168,496,141 shares at 117.6232; duplicate and unknown refs flagged). Other types are checked only against spec offsets, not run through a second decoder.
- **FastMM:** its tests use its own encoder, which has this blind spot; it did agree with CppTrader on a real file. **Verdict:** adopt as test.

### Pool-based message enrichment
- **Where:** ITCH repo (Apache-2.0): keep live orders so E/C/X/D carry side, size, price, MPID.
- **Caveats:** `double` prices as map keys; `pool[id]` silently creates entries; partial protocol; offline only.
- **FastMM:** the bridge already resolves orders from its book; `features()` covers research output. **Verdict:** ignore.

### A/B arbitration via one shared gap buffer
- **Where:** udp-multicast-receiver (concept only); FastMM `Receiver<H>` (MIT).
- **What:** both lines feed one buffer; first-valid-wins and duplicate drop come free; a loss on one line produces zero gaps, a loss on both exactly one gap with the correct range.
- **FastMM:** has A/B arbitration with a reorder buffer, re-request from `on_timer()` and loss tests. **Verdict:** has it.

### Reorder window sized as time
- **Where:** udp-multicast-receiver; hftkit `net/sizing.hpp`.
- **What:** slots are *messages*, so a 4,096-slot window lasts 4096/R seconds: ~4 ms at 1M msg/s, ~0.4 ms at 10M, shorter than a typical retransmit round trip. State it in config docs and count overflows.
- **Evidence:** arithmetic `verified` by hftkit tests.

### Verified receive buffer
- **Where:** udp-multicast-receiver: the kernel clamps `SO_RCVBUF` to `rmem_max`; try `SO_RCVBUFFORCE`, then `SO_RCVBUF`, read back what was granted, expose the shortfall.
- **FastMM:** not checked whether its UDP source reads back the granted size. **Verdict:** adopt as test.

### Kernel timestamp diagnostic
- **Where:** udp-multicast-receiver `timestamp_validate`: loopback send→kernel RX stamp p50 625 ns, kernel stamp→user read p50 1.24 µs (`claimed`). **Verdict:** adapt as a start-up self-check.

### Retransmit pacing
- **Where:** udp-multicast-receiver: wait `gap_timeout_ns` (100 µs) before the first request, retry every 1 ms, cap 100 messages per request. **Verdict:** compare with FastMM's timer logic.

### LLEP wire design points
- **Where:** low-latency-event-engine `docs/protocol.md` (MIT): global per-session packet sequence (NACK needs no stream id), retransmit exact original bytes, all-or-nothing frame validation, events never split, 1,416-byte cap for IPv4/IPv6 MTU.
- **Caveats:** recovery is specified but not implemented; BestEffort only. **Verdict:** adopt as design ideas.

### CME MBO fill classification (`FillSlate`)
- **Where:** kaspar-hft (MIT).
- **What:** an MBO trade record has order id and quantity but no price or side; a modified-to-cross order is still at its old price until its Change arrives after the trade. Hold each fill until its own order's record arrives, then classify: one side only → all RESTING; one order against many → the lone order is AGGRESSOR; many both sides → RESTING (auction uncross); one-v-one → price-change then touch test; else AMBIGUOUS (0.19–0.71% of fills). Count distinct orders, not fills.
- **Evidence:** `read`; the code cites real ZN/ES/NQ cases. **Verdict:** adopt as design for any CME MBO path.

### MDP3 recovery
- **Where:** kaspar: snapshot and instrument-recovery actors resync on gaps; serial decode beat parallel by 1.5–3× at p99 (packets carry ~1.06–1.10 messages). `claimed`, an honest negative result. **Verdict:** note.

### Hyperliquid and Lighter connectors
- **Where:** upstream FastMM 0.7.0. Signing is checked against golden fixtures (Hyperliquid) and the official Go signer (Lighter) rather than round-tripped through the repo's own code.
- **Evidence:** `read` of diffs only, not built. **Verdict:** same lesson as golden vectors.
