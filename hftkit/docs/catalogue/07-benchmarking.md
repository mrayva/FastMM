# 7. Benchmarking and measurement

### Open-loop latency from the intended send time
- **Where:** revoq `workload.md`; hftkit `bench/open_loop.hpp`.
- **What:** latency = completion − *intended* start. When the system stalls, later requests that were due during the stall record their full wait, so the stall is not hidden (coordinated omission). Unpaced runs report throughput only.
- **Evidence:** hftkit `verified`: a 20 ms stall at request 400 at 10 kHz gives ≥100 samples above 1 ms and max ≥15 ms, where timing each call alone shows only a handful.
- **FastMM:** `bench_latency.cpp` and `bench_spsc.cpp` have not been checked against this rule. **Verdict:** adopt as test.

### Layout-seed multi-process runs
- **Where:** low-latency-event-engine `run_clean.sh`: many processes with ASLR on and a different `LLE_LAYOUT_SEED` that moves queue objects and buffers to a random page offset; statistics and confidence intervals across processes. A single-process ASLR-off mode is kept for repeatability. Round trip timed with fenced `rdtsc` on the initiator core only.
- **Evidence:** `read`. **FastMM:** ring benchmarks run in one process. **Verdict:** adopt.

### Honest scope statements
- **Where:** Pulse-Order (MIT): the measurement boundary is stated (clock starts after `rte_eth_rx_burst` returns, stops after `rte_eth_tx_burst` accepts), timer overhead sampled (100k samples), 100k warm-up then 1M measured events per regime (NO_SIGNAL/BUY/SELL), every returned order validated, and the README lists what the numbers exclude (no physical NIC, no hardware timestamps, no real bursts). Published: Ring PMD p50 111 ns / p99 552 ns; AF_PACKET/veth p50 1.74 µs / p99 3.26 µs (`claimed`).
- low-latency-event-engine `performance.md` opens with "No representative results yet" and records negative and reverted experiments (LTO not adopted).
- **Verdict:** adopt as the template.

### Durability as a benchmark axis
- **Where:** revoq: `none`, `page_cache`, `fsync`, `archive` are separate chart series, never combined; Java drivers must reach JIT steady state and a GC cycle first; results include command lines, kernel, topology, governor, hugepage state and a lossless HdrHistogram. orderer-cpp's gate uses a real durability primitive at least every 1024 records. **Verdict:** adopt.

### Gate on scaling, not absolute speed
- **Where:** orderer-cpp `BENCH.md`: fixed seeded W6 workload that may never change once published; producer k publishes `symbol % N == k` so the workload is identical for every N; latency is publish-to-first-event with every sample stored; timed and untimed throughput reported separately because two clock reads cost about one command on an M1. **Verdict:** adopt.

### Worst-case headline, grouped by structure
- **Where:** matching-engine-benchmark: headline is the lowest of five scenario medians; engines grouped by ladder structure with the caveat that language confounds. It is vendor-published and its target engine is not in the repo. **Verdict:** adopt the "worst of scenarios" rule.

### Latency attribution by cause
- **Where:** Limit-Order-Book: record order type, latency, executed-order count and rebalance count per request, then separate them in a 3D plot. hftkit does not include this; it is a harness pattern. **Verdict:** adopt in FastMM `bench/`.

### Depth-scaling sweeps
- **Where:** lob-engine `bench_depth`: add/cancel at 1k, 8k, 60k live orders, longest cluster and page faults as columns (add 79–124 ns, cancel 53–65 ns `claimed`).
- **FastMM:** its book benchmarks use fixed working sets; a sweep is not yet done. **Verdict:** adopt.

### Measured comparison: FastMM vs CppTrader on `sample.itch` (this project)
`verified`, 4-vCPU shared Xeon at 2.1 GHz, tens of percent run-to-run variation; the sample is 1,563,071 messages, 77% NOII, pre-market.

| | here | published (i7-4790K, full day) |
|---|---:|---:|
| CppTrader parser, in RAM | 10–11 ns/msg | 24 |
| FastMM decoder (all msgs) | 4.3 ns/msg | n/a |
| CppTrader reference manager | 28–46 ns/msg | 309 |
| CppTrader optimized | 14–19 ns/msg | 120 |
| FastMM bridge, first 1M msgs | 76–89 ns/msg | n/a |
| FastMM bridge, whole slice | 730–780 ns/msg | n/a |

Book state identical on all 2,475 symbols at 1M messages. The slow tail is the recentre storm (section 2). The bridge also builds FIFO queues and emits top-20 deltas, which CppTrader's optimized variant omits.

### Reproducing someone else's number
- ultra-low-latency-orderbook: claimed 5.93 ns/op but the benchmark source is absent. submicro: injected `busy_wait_ns` and a 400 ns software "FPGA". These are the cases where the right response is to ignore the figure.
