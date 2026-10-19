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
