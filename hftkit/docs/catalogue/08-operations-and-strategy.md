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

### Kernel state machine for permission to add risk
- **Where:** raikiri `oms/kernel_gate.hpp` (concept only), a constexpr function from (state, event) to state.
- **What:** Running, Degraded, Halted. A dropped execution record or a stalled gateway heartbeat degrades (and never promotes out of Halted); a lost gateway or a confirmed reconciliation failure halts; an agreeing reconciliation clears Degraded but only records the agreement when Halted; an operator resume is refused unless a reconciliation has agreed since the halt. Cancel is allowed in every state.
- **Verdict:** adopt as a design. It is small enough to test exhaustively.

### Reconciliation that needs a second boundary
- **Where:** raikiri `oms/recon.hpp`.
- **What:** the venue's position is compared with the book as of the venue's own timestamp, by rewinding fills booked after it (`book_as_of`). A first disagreement is only noted; it becomes a halt for that instrument when it is seen again at a different venue boundary. "Venue cannot report", "pull failed" and "venue reported zero" are three states, so a failed read is never taken as flat.
- **Verdict:** adopt.

### Risk gate as a pure function
- **Where:** raikiri `oms/risk.hpp`: `check(cmd, exposure)` with no hidden state, so replay gives the same verdicts. Refuses on a reconciliation break first, then bad instrument, quantity, lot ceiling per market kind (a fat-finger backstop), limit price, a price band checked by cross-multiplying (tick counts truncate if divided first), post-only on a market order, open-order count, long and short limits (same-side working orders count as filled), and three notional limits (order, instrument, strategy; "unpriced exposure" refuses rather than guesses). Cancels never consult it. Compare Pulse-Order's divide-before-multiply notional cap and FastMM 0.7.0's through-zero fix. **Verdict:** adopt.

### Rate budgets with cost, reserve and backoff
- **Where:** raikiri `exec/rate_budget.hpp`, `common/sliding_log.hpp`.
- **What:** a limit is a sliding log of (time, cost); `reserve` is a slice only order traffic may spend; a request carries a cost found by the longest matching path rule (a rule may require `?k=v` pairs in the query); it spends from every limit its path matches or from none; a venue refusal holds a limit (or all of them) for a backoff; limits are set near 90% of the venue's figure so a venue that counts fixed windows is also satisfied. Kinds beyond REST: websocket messages, connects per ip, streams per socket, sockets per ip, channels and subscriptions per frame.
- **Caveats:** one mutex for the whole budget, because all clients share the per-ip allowance.
- **Verdict:** adopt the model.

### Venue clock as hard bounds
- **Where:** raikiri `introspect/offset_bounds.hpp`, `clock_probe.hpp`.
- **What:** the offset between a venue's clock and ours is kept as an interval, never a point. A REST probe gives bounds from the send and receive times; the feed gives a lower bound from the largest `ts_event − ts_recv` seen; intervals are intersected; the correction is the point nearest zero, so our clock is moved only as far as the bounds force. An empty intersection is reported as `Conflict`. A burst of probes is taken hourly (every minute after a failure). Lag is kept in a signed log-bucketed histogram (four buckets per power of two up to about 67 s) over a rolling minute.
- **Verdict:** adopt for any feed with a venue timestamp.

### Funding and fee accounting
- **Where:** raikiri `oms/funding_split.hpp`, `funding_identity.hpp`, `fee_ledger.hpp`: a funding charge is split pro rata by current size, shares sum exactly to the charge (the remainder goes to the last holder, unheld charges to a manual book); a settlement's dedup key is packed instrument, settlement second and strategy, because the socket and the REST ledger mint different ids for the same charge; booked fees are held until the venue's ledger quotes them back and the verdict is agreed, disagreed or unmatched. **Verdict:** adopt.

### Engine stall watchdog, notify units and log reopen
- **Where:** upstream FastMM 0.9.0 (`read`): `[engine] stall_ms` (default 10,000) detects an engine thread that stops advancing its heartbeat and cancels all orders over REST on every venue, writes the kill and status files and exits 5 without joining the engine; a long stop of the whole process is not counted against the engine. `fastmm-live` and `fastmm-gateway` speak `sd_notify` (ready, watchdog, stopping) and the units are `Type=notify`; `SIGUSR1` and `fastmm-ctl log-reopen` reopen the log for logrotate; a shutdown waits for the venues' confirmations, not fixed sleeps; REST orders and cancels get their own connection so they never wait behind a snapshot; DNS lookups run on at most four shared threads off the pinned cores; each connection draws its own reconnect jitter; `max_loss_period = "utc_day"` makes `max_loss` a daily budget. **Verdict:** adopt as a checklist for a live process.

### Credentials as references
- **Where:** raikiri: credentials are passed as `env:NAME`, `file:/path`, `exec:command` or `op://vault/item/field`, never as values, since anything on a command line is visible to every user through `ps`. **Verdict:** adopt.

### Process-per-venue isolation
- **Where:** raikiri: one gateway process per (venue, market); a crashed or banned gateway takes nothing else down; the kernel reads every segment and keeps one book. The daemons do not pin to cores yet (README), and the 167 ns figure comes from an unpinned run.
- **Verdict:** compare with FastMM's `fastmm-gateway`, which also separates exchange connections from strategy processes.
