# 5. Simulation and backtesting

### Policy-templated engine: one strategy, three modes
- **Where:** FastMM `Engine<Strategy, Clock, Transport, Feed>` (MIT): live, backtest, replay. No virtual calls in `run()`.
- **Compare:** roq-algo reaches the same goal through runtime interfaces (Strategy/Matcher/Reporter, strategy sends live-style orders into a simulated exchange); kaspar through config (SOM/OB for simulation, iLink/`TachBook` live).
- **Verdict:** FastMM's compile-time approach is the strictest; the others are alternatives, not upgrades.

### Queue-position fill model
- **Where:** FastMM `core/queue_model.hpp`.
- **What:** join at the back of the displayed queue; a level shrink is split between ahead and behind scaled by `(1 − conservatism)`; trades consume the queue ahead first, trade-throughs fill with the printed quantity; own orders are subtracted from displayed levels; a newer BookTicker caps the queue and the trade tape corrects stale depth; an order only fills from trades after it reached the venue. The same functions drive the live estimate, the simulator and the fill checker.
- **Calibration:** `queue_conservatism` by grid search on error rate, cross-validated across sessions; latency per venue by maximum likelihood on millisecond timestamps, inverting lognormal percentiles.
- **Evidence:** `read`. **Verdict:** the reference; roq-algo's version is an unfinished sketch.

### Lognormal latency with drop probability
- **Where:** FastMM: fixed plus lognormal excess with mean `jitter` (E[excess] = jitter for any sigma), seeded Xoshiro256**. roq-algo uses a fixed millisecond delay the matcher does not even apply; TALON draws normal latencies around 1,500 ns floored at 500.
- **Verdict:** FastMM's is the richest.

### Exact MBO queue plus FillSlate
- **Where:** kaspar `OB` and `FillSlate` (section 3). Exact price-time position for simulated passive fills when the feed is MBO. **Verdict:** adopt as design for FastMM's ITCH/L3 path.

### Model-free passive execution (shadow-POV)
- **Where:** kaspar `SHADOW_ALGORITHM.md`.
- **What:** on a third-party ADD at a price, place an own order there and record the association; cancel when the shadowed order is cancelled (at once) or traded (after a short grace), or when position hits target, working size exceeds distance to target, the level is crowded, or price drifts. Placement throttled (every N-th add or 3% chance). No fill prediction.
- **Evidence:** the paper's year-of-ES evaluation `claimed`. `SlippageProbe` documents inventory pinning at band edges (|pos| ≥ 90% of band on 26% of fills). **Verdict:** adapt as a strategy for comparison.

### Shadow book and measured look-ahead leakage
- **Where:** talon-kernel (AGPL-3.0, concept only).
- **What:** a second book rebuilt from the published feed; reactive agents read only it. Comparing it with the authoritative book across 14.24M evaluations found 3.17M mismatches (22.24%), mean exchange lead 7.11 µs, max ~473 µs (`claimed`; one scenario).
- **Caveats:** the event queue silently drops events when full; `l2_ns` use is commented out in the loop; the figure is a demonstration of the mechanism, not a constant.
- **FastMM:** whether any strategy-visible state (e.g. `venue_mid()`) is read from the matching engine rather than the published stream is unchecked. **Verdict:** reimplement the check, not the code.

### Look-ahead-safe research features
- **Where:** FastMM `features()`: forward mids with a strict no-look-ahead rule, unresolved rows excluded not filled with the last mid; rolling Spearman IC and bucketed markouts; cross-check ties each fill's forward mid to the runner's markout.
- **Verdict:** has it.

### Event queue tie-break
- **Where:** TALON orders by `(timestamp, sequence_num)`; FastMM `event_scheduler.hpp` needs the same total order. **Verdict:** compare.

### Event heaps and bitmaps for simulation speed
- **Where:** upstream FastMM 0.7.0: per-instrument bitmap of own orders, 4-ary heap of 16-byte entries with an arena and prefetch, flat open-addressing `OwnQuantity`.
- **Evidence:** `read` only; no timings were in the commits and the benchmarks were not rerun. **Verdict:** note.

### Zero-intelligence, market-maker and momentum flow
- **Where:** TALON's 100 ZI / 20 MM / 5 momentum agents. **Verdict:** reimplement if a reactive background is wanted.

### Hawkes intensity and Avellaneda–Stoikov quoting
- **Where:** submicro-execution-engine (proprietary, concept only). Recursive exponential-kernel Hawkes `λ(t) = (λ_prev − μ)e^(−β dt) + α + μ`; A–S reservation price `s − qγσ²(T−t)` and spread `γσ²(T−t) + (2/γ)ln(1+γ/k)`.
- **Evidence:** formulas are standard and the repo's derivation is correct (`read`); its latency numbers are simulated (see the ledger). **Verdict:** reimplement from the published models if wanted.

### Position tracker with split-at-zero
- **Where:** roq-algo `PositionTracker`: a fill that flips a position is split at zero so realized P&L is booked on the closing part; `price_to_ticks` refuses precision loss. **Verdict:** adopt the two ideas.

### Parameter A/B arms and branch backtests
- **Where:** upstream FastMM 0.8.0–0.9.0 (`read`): `[ab]` runs parameter arms by instrument, time block or crossover, live and in backtests; every order carries its arm into the journal and store; `fastmm-data ab` compares arms with paired, resampled intervals and weighs each pair by the time it ran; `branch` assignment runs every arm over the same data and seed; sweep points, walk-forward folds and branch arms start from one strategy state and none writes its end state back.
- **Verdict:** adopt as a design for strategy comparison without look-ahead or state leakage.

### Resting-order fills when the recorded book trades through
- **Where:** upstream FastMM 0.9.0 `sim` (`read`): under `l2_queue` an opposite level that arrives at or through a resting order fills it; post-only is checked against a book ticker newer than the depth; a reduce-only order is refused on a flat or growing position; a FOK under self-trade prevention fills whole or not at all; an inverse contract's entry is the harmonic mean of its fills; a spot market buy is held at the levels it would take. These are venue behaviours a simulated account has to reproduce, listed as fixes.
- **Verdict:** use as a checklist for any simulated venue.

### Execution algorithms as a state machine over parent and child orders
- **Where:** raikiri `libs/joe` (concept only).
- **What:** a parent goes pending → working → done / cancelled / failed (working ↔ paused); a fill is booked in any state, so one landing after a cancel still counts; an overfill reads as zero leaves, never negative. TWAP, a U-shaped weight (`1 + 2(2x−1)²` at each slice midpoint), a VWAP weight from a 48-bucket (half-hour) volume curve, and POV. Slice times are jittered by a seed, so a parent's schedule is identical on every call. The cumulative target makes the last slice land exactly on the total, and a shortfall is caught up only to a percentage of the planned slice until the last. The smart order router splits a quantity across venue books by price after taker fee, in whole lots, with each leg clearing the venue's minimum notional and an IOC limit at the deepest level it reaches.
- **Evidence:** `read`; pure functions, easy to test.
- **Verdict:** reimplement if FastMM needs parent-order execution.
