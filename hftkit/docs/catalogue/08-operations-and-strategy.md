# 8. Operations and strategy

### Start-up refusals on untrusted state
- **Where:** upstream FastMM 0.7.0 (`read`, not built or run). With API keys it requires `max_position` (or `max_order_notional`) and `max_loss`, or an explicit `allow_unlimited`; turning a limit off needs `--force`. It exits 9 on a restored position the newest session booked twice (override `--accept-position`), on a missing epoch file while the store holds sessions of that engine, and exits 6 on a lost kill file that would clear a recorded `max_loss` trip. Store failures show in the status file and metrics and can trip the kill switch.
- **Caveats:** several are breaking (existing configs may stop starting); a large share of the commits name an AI co-author, so diff review is the safeguard for the safety logic.
- **Verdict:** adopt as a pattern: refuse rather than guess.

### Silent-failure guards from bug history
- kaspar: startup assert on array size; `pop_batch` capped at 8192 so it returns under overload; polled-wait safety nets. TALON: the opposite lesson, a queue that silently drops on full.
- **Verdict:** every capacity limit should either assert at construction or count and report.

### Risk checks committed with validation
- **Where:** Pulse-Order `RiskGuard`: position and outstanding count updated in the same call that validates; notional cap computed as `limit / price` so it cannot overflow; overflow-safe absolute value (`-(v+1)+1`); typed status (no order, emitted, invalid, risk rejected, outbox full) and the client id advances only on a successful push.
- Upstream FastMM 0.7.0 fixed caps so an order that crosses through zero checks only the closing part as reducing and the rest as opening exposure.
- **Verdict:** adopt.

### Duplicate-execution memory and estimated fills
- **Where:** upstream FastMM: a Binance spot replay re-sent rows 4,000+ executions late and was double-booked; memory grew 4,096 → 65,536 executions with a test replaying after 30,000 others. Estimated fills moved from a 64-entry table into the order's own slot; losing one is reported (`forgotten_qty`), not dropped. Sizes are empirical.
- **Verdict:** note: size dedupe memory from observed replay depth.

### Quote management
- **Where:** FastMM `QuoteManager`: diff desired vs resting quotes to a minimal New/Cancel/Replace set; hysteresis (`min_requote_ticks`, `min_qty_bps`) and a minimum interval; reject backoff doubling to a cap; cancel-then-new where no replace exists; FIFO of rate-limit-starved instruments served as tokens return.
- **Verdict:** has it.

### Snapshot-plus-delta resync
- **Where:** FastMM `BookSyncer`: Idle → Buffering → Synced → Resync with a fixed byte arena and a stated reason (gap, overflow, snapshot too old). slick-orderbook only offers `emitSnapshot` begin/end brackets and trusts the caller's sequence. **Verdict:** FastMM's is the one to use.

### Host tuning and diagnostics
- low-latency-event-engine `tune.sh` applies and restores governor, turbo, SMT and EPP, saving state in `/run`.
- submicro jitter profiler: cycle gap between busy-wait iterations to spot SMIs and preemption. Method is sound (`read`); a candidate start-up check of core isolation.
- lob-engine: apply `SCHED_FIFO` only when the core pin actually succeeded; two busy-poll FIFO threads on one CPU split iterations 8,800:1.
- udp-multicast-receiver: verify `SO_RCVBUF`.
- **Verdict:** adopt the scheduling rule and the jitter check.

### TSC clock calibration
- **Where:** FastMM: 32.32 fixed-point rate against `CLOCK_MONOTONIC_RAW`, re-measured over the whole interval since the last anchor, published through a seqlock, bounded drift correction ≤500 ppm, steps only if >1 ms off. **Verdict:** has it.

### Release engineering
- **Where:** slick-orderbook: publish the packages CI already built for the tagged commit, check tag against CMake version, build releases without LTO and `-march=native`, generate the version header from `project(VERSION)`. **Verdict:** adopt if FastMM ships binaries.

### Python hooks at native speed
- **Where:** FastMM: `@fastmm.hot` compiled with Numba against a numpy mirror of a C ABI header, IR check before execution. **Verdict:** has it.

### Actor framework with inline `fast_send`
- **Where:** kaspar: inline handler call under a per-actor mutex returns the reply by value, interchangeable with async send (README ~7.5 ns over a direct call, `claimed`); can deadlock on cycles, formalised in a tech report. Mailbox variants (BQueue, sharded, Vyukov MPSC with a 1 ms polled wake) were benchmarked on EPYC. **Verdict:** note.

### Batch hand-off between threads
- **Where:** order-matching-engine: producers batch 256 orders per shard and push in one `memcpy`. FastMM rings move one message at a time. **Verdict:** test only if per-message cache-line cost shows in a profile.

### Fixed-point numerics
- **Where:** FastMM: distinct `Price`/`Qty`/`Notional` int64 types at 1e-8 so `p*q` does not compile, `mul_div` stays 64-bit when the product fits (avoiding `__divti3`, ~40 cycles), venue strings parsed as exact decimals. ITCH repo and roq-algo use `double`. **Verdict:** FastMM's is the one to copy.
