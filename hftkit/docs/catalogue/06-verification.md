# 6. Verification

### Differential testing against a slow reference book
- **Where:** lob-engine (no licence, concept only); hftkit `check/ref_book.hpp`.
- **What:** `std::map<Price, deque<Order>>` written from the rules, compared after every event on the exact fill sequence, best bid/ask, no crossed book, and the conservation identity; the model mirrors the engine's capacity limit so a boundary is not mistaken for a bug. lob-engine runs 5 seeds × 1M events in CI and a 10M job under ASan/UBSan/TSan (`claimed`).
- **Evidence:** hftkit's own random run of 200,000 operations checks the ledger and crossed state after each event `verified` (against itself; it is an oracle for other engines, not a self-test of those).
- **Verdict:** adopt; use it on `sim/matching_engine.cpp`.

### Quantity conservation identity
- **What:** `submitted == 2·filled + resting + cancelled + rejected + expired` (each fill consumes quantity from both sides). hftkit `check/conservation.hpp`. **Verdict:** adopt.

### Structural test instead of a timing test
- **Where:** lob-engine #16. A timing test was tried first and the broken hash passed it; asserting the longest probe cluster caught it. hftkit `check/cluster.hpp` and `test_open_index`. **Verdict:** adopt for `OpenHashMap` and `L3RefIndex` with sequential ids (the ITCH case).

### Byte-identical oracle across engines
- **Where:** matching-engine-benchmark (MIT): SHA-256 of the full report stream, established first by three independent engines; 160 of 247 surveyed engines reproduce it across 100 seeds, 1B+ messages each (`claimed`). `explain_divergence.py` finds the first differing sequence number; 181 issues filed, 25 fixed (`claimed`).
- **FastMM:** `sim/matching_engine.cpp` could sit behind its C ABI. **Verdict:** adopt as design.

### 34-case conformance gate
- **Where:** matching-engine-benchmark: edge cases each reverse-engineered from a field bug, e.g. a trade prints at the maker's price in both directions; cancelling a middle FIFO order must not lose those behind it; cancel of a filled order is rejected; a marketable limit stops at its own price; a cancelled best level leaves no phantom price.
- **Evidence:** `read`; the real stale-best-ask bug in order-matching-engine is one it would catch. **Verdict:** adopt as test battery.

### Anti-cheat probes
- **Where:** matching-engine-benchmark: perf and audit are separate passes with identical random probes (`best_bid`, `best_ask`, `depth_at`), only the audit run compares answers, probe time is excluded; empty-book assert and `engine_flush()` inside the timed window; four executable cheat adapters show each defence working. **Verdict:** adapt idea for replay verification.

### Spec-first, vector-checked ports
- **Where:** orderer-cpp: spec vendored with a SHA-256 manifest and a verify script; every language port must reproduce the same bytes (event listings, journals, snapshots, exit codes); diff fuzzer over 8 seeds, depth-3 exhaustive enumerator, 63-way cross-restore test (upstream, `claimed`). **Verdict:** adopt as pattern for codecs (ITCH, OUCH, SBE).

### Hand-typed vectors, checked to bite
- See section 3. The extra step in udp-multicast-receiver is verifying the new vectors *fail* against the old buggy decoder.

### Allocation counter and page-fault counter
- **Where:** orderer-cpp `no_alloc_test` (replaces global `operator new`, 0 allocations over 1M commands; reaching zero needed a fix because `submit_tagged` built a `std::function` and an `OrderBook` per call); FastMM has the same style of test; lob-engine counts minor faults across the matching loop. hftkit `check/alloc_counter.hpp`, `bench/pagefaults.hpp`.
- **Evidence:** hftkit test `verified` that OpenIndex, ClockCache, BitLadder, AckGate and the record log do not allocate in steady state.
- **FastMM:** has the allocation test; lacks the fault test. **Verdict:** adopt.

### Sanitizer matrix
- **Where:** lob-engine (ASan/UBSan/TSan), low-latency-event-engine, orderer-cpp (TSan: 30/30 passes), ultra-low-latency-orderbook (optional ASan/UBSan, `-Wconversion`). hftkit: ASan+UBSan option, six repeated passes. **Verdict:** adopt.

### Bug ledgers as documentation
- **Where:** lob-engine (18 numbered bugs, each with how it was found); slick-orderbook 1.1.0 changelog (stale top of book after clear, wrong `num_orders`, `auto&` vs `auto&&` on `flat_map` iterators, implicitly deleted move operations, each with a regression test); orderer-cpp upstream fixes; TALON priority-queue comments.
- Two entries worth remembering: lob-engine #14, a non-volatile accumulator let the optimiser delete a TSC calibration loop (0.08–0.32 GHz on a 2.1 GHz machine), which overturned a reported 1.29–1.45× SoA speedup to ~1.02×; and #18, tests that could not fail. **Verdict:** keep a ledger for FastMM.

### README numbers checked by a script
- **Where:** lob-engine `scripts/update_readme.py` refuses a README benchmark block that drifts from `BENCHMARK_RESULTS.md`; upstream FastMM now checks reference outputs in the docs against the build. The slick-orderbook contradictions in the ledger are the failure case. **Verdict:** adopt.
