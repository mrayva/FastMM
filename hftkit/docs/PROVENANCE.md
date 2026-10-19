# Provenance

hftkit was written from scratch. The ideas below were identified by reading the listed
repositories (all under github.com/mrayva/ or the FastMM upstream); **no source text was
copied**. Repositories that are AGPL, proprietary, or carry no licence were used for
understanding only, and their code was neither copied nor closely paraphrased.

| Component | Idea seen in | Notes |
|---|---|---|
| Fibonacci hash, partition routing | FastMM, kaspar-hft | Constant is the public 2^64/phi value; reference values computed independently |
| Cluster-length structural test | FastMM / hash-map tests | Test demonstrates why a low-bits-after-multiply hash collapses |
| `OpenIndex` backward-shift erase | FastMM (`OpenHashMap`) | Standard algorithm (Knuth 6.4, Algorithm R) |
| `BitLadder` | FastMM L3 book bitmap, order-matching-engine | Textbook two-level bitmap |
| `ClockCache` | LruClockCache | Flags are explicit, avoiding the sentinel-key-0 bug found while analysing it |
| `record_log`, CRC-32C | FastMM journal, low-latency-event-engine | Format here is original; CRC polynomial is the public Castagnoli one |
| `AckGate` | FastMM / revoq | Durable vs journaled watermark idea |
| `sizing` | udp-multicast-receiver / MoldUDP64 discussion | Formula only |
| `QtyLedger`, `RefBook` | order-matching-engine, Limit-Order-Book | Differential-testing pattern; rules are hftkit's own |
| `alloc_counter`, `pagefaults` | matching-engine-benchmark | Standard techniques (operator new replacement, getrusage) |
| `open_loop` | matching-engine-benchmark, submicro-execution-engine | Coordinated-omission-safe measurement; method is public |
| Wire vectors | NASDAQ TotalView-ITCH 5.0, MoldUDP64 specs | Typed from the specs; Add cross-checked against FastMM's decoder |

AGPL (talon-kernel), proprietary (submicro-execution-engine) and unlicensed repositories
(lob-engine, udp-multicast-receiver, ultra-low-latency-orderbook) contributed *concepts only*.
