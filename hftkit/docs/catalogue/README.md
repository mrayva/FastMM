# Annotated catalogue of techniques

A reference to the notable techniques, data structures and methods found in FastMM and 18 related
repositories, written from the analyses done in this project. It is documentation only; the
code that came out of it is in `../../include/hftkit` (see `../PROVENANCE.md`).

## How to read an entry

| Field | Meaning |
|---|---|
| **Where** | Repository, licence, and the component or file. Commits were not pinned: each repo was read at its mirror's state as of 2026-10 (upstream FastMM at `f76e871` for the re-analysis). |
| **What** | The idea in a few lines. |
| **Evidence** | `verified` = I ran it or reproduced it in this project. `read` = I read the code or docs and found it consistent, nothing was run. `claimed` = the repo's own number or statement, not reproduced. |
| **Caveats** | Known problems, scope limits, bugs I found. |
| **FastMM** | `has it` / `better there` / `adopt as test` / `adopt as design` / `n/a`. |
| **Verdict** | `adopt`, `adapt`, `reimplement` (idea only, because of licence or quality) or `ignore`. Where hftkit implements it, the header is named. |

## Sections

1. [Memory and containers](01-memory-and-containers.md)
2. [Order books and matching semantics](02-books-and-matching.md)
3. [Feed handling](03-feed-handling.md)
4. [Persistence and replay](04-persistence-and-replay.md)
5. [Simulation and backtesting](05-simulation-and-backtesting.md)
6. [Verification](06-verification.md)
7. [Benchmarking and measurement](07-benchmarking.md)
8. [Operations and strategy](08-operations-and-strategy.md)

Updated 2026-10-25: raikiri and turboq entries added, FastMM 0.8.0–0.9.0 changes folded into sections 1, 2, 4, 5 and 7.

## Repositories covered, and what may be copied

Licences are as recorded during the original analyses (README or LICENSE file at the time).
Check the upstream repository before copying anything.

| Repo | Role | Licence | Copy? |
|---|---|---|---|
| FastMM | full market-making engine (upstream 0.9.0 re-reviewed 2026-10-25) | MIT | with notice |
| kaspar-hft | CME stack and simulator | MIT | with notice |
| roq-algo | strategy/matcher interfaces | BSD-3 | with notice |
| matching-engine-benchmark | cross-engine benchmark and oracle | MIT | with notice |
| Pulse-Order | DPDK hot-path micro-benchmark | MIT | with notice |
| ITCH | offline ITCH-to-CSV | Apache-2.0 | with notice |
| Limit-Order-Book | AVL LOB with stops | MIT | with notice |
| order-matching-engine | sharded matcher | MIT | with notice |
| CppTrader | matcher and ITCH parser | MIT | with notice |
| low-latency-event-engine | SHM/UDP transport | MIT | with notice |
| LruClockCache | CLOCK cache | MIT | with notice (but see the key-0 bug) |
| orderer-cpp | partitioned pipeline | MIT or Apache-2.0 | with notice |
| revoq | mmap IPC journal | Apache-2.0 | with notice |
| slick-orderbook | L2/L3 library | MIT | with notice |
| raikiri | multi-process crypto trading framework | LGPL-3.0 | **no** into MIT code (reimplement from the idea) |
| turboq | shared-memory SPSC/MPSC/multicast queues | MIT | with notice |
| ultra-low-latency-orderbook | fixed L2 book | none found | **no** |
| lob-engine | single-symbol LOB | none found | **no** |
| udp-multicast-receiver | MoldUDP64 receiver | none found | **no** |
| talon-kernel | agent-based simulation | AGPL-3.0 | **no** (copyleft) |
| submicro-execution-engine | research pipeline | proprietary, no commercial use | **no** |

Repos in the "no" rows were used for understanding only. Everything attributed to them below was
reimplemented from the idea or not used at all.

## Claim versus evidence ledger

Claims that did not survive reading, or that I could not check.

| Source | Claim | Finding |
|---|---|---|
| submicro-execution-engine | 890 ns end-to-end | Built from `busy_wait_ns` injections and a fixed-latency stub; its own `LATENCY_BUDGET.md` says 2.75 µs. |
| order-matching-engine | "lock-free SPSC ring" | It is a spinlock-guarded circular buffer. |
| order-matching-engine | ~160M orders/s | Aggregate over shards on synthetic load; the benchmark repo measured 3.28 M/s worst case for the matching core. |
| slick-orderbook | all targets exceeded; <10 KB per 1,000 L3 orders | Its own PERFORMANCE.md says ~88 KB; 0.25 ns best-bid "latency" is about one cycle, so the load was hoisted. |
| ultra-low-latency-orderbook | 168 Mops/s | Benchmark source absent from the repo; unverifiable. |
| udp-multicast-receiver | "production-grade" | Synthetic data, five opcodes, no real capture; decoder first had the wrong ITCH layout and passed 202 assertions. |
| Limit-Order-Book | rebalance is O(log M) | `getLimitHeight` walks the subtree; measured ~2.5 µs per rebalance. |
| LruClockCache | working write-back cache | Key 0 is lost while unused slots are consumed (reproduced). |
| raikiri | 167 ns cross-process p50, "43 live venues", "23 with order entry" | 167 ns is an M-series laptop; I measured 414–1,006 ns p50 on a shared 4-vCPU VM (not comparable). The docs say 24 order-entry venues in one place and 23 in another; 20 of the 43 are market-data only; the README says not every order path has run on a real account. |
| turboq | lock-free multicast | Documents that a lapped consumer may read skipped, torn or garbage messages; publishes no numbers. |
| FastMM (my earlier finding) | recentre cost grows with window | Fixed upstream in 0.9.0; re-measured (section 2). |
| Pulse-Order | (none) | Scope and exclusions are stated honestly; used as the model for measurement hygiene. |
| lob-engine | (none contradicted) | Numbers not reproduced; its 18-bug ledger is the useful part. |
| revoq, low-latency-event-engine, CppTrader, kaspar | published numbers | Not reproduced, except CppTrader's parser/book which I re-ran (section 7). |
