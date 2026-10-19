# 1. Memory and containers

### Open-addressing index with backward-shift deletion
- **Where:** FastMM `OpenHashMap` / `L3RefIndex` (MIT); hftkit `ds/open_index.hpp`.
- **What:** Linear probing, power-of-two capacity, fails above 7/8 load instead of rehashing, deletes by shifting later cluster members back, so there are no tombstones. FastMM deliberately offers no iteration, because hash order would break replay.
- **Evidence:** `read` (FastMM); hftkit version `verified` against `std::unordered_map` over random and churn workloads.
- **Caveats:** Capacity is fixed; a table that fills returns failure.
- **FastMM:** has it. **Verdict:** adopt the pattern (done in hftkit).

### Fibonacci hashing, top bits
- **Where:** orderer-cpp routing spec (MIT/Apache); lob-engine bug #16 (no licence, concept only); hftkit `hash/fib.hpp`.
- **What:** `h = key * 0x9E3779B97F4A7C15`, then take the *top* bits (`h >> (64 - bits)`). Using `(id * 2654435761) >> 32` instead is roughly `0.618 * id`, so sequential ids land in one giant cluster (lob-engine: 60,000 live orders, 3.5 ms per cancel at 8,000).
- **Evidence:** lob-engine `claimed`; hftkit test `verified`: the bad hash gives a cluster over 10,000 for sequential ids, the top-bits hash stays under 64.
- **FastMM:** has the multiply-shift home slot; the cluster test is missing there. **Verdict:** adopt as test.

### Multiply-shift partition routing
- **Where:** orderer-cpp `ROUTING.md`; hftkit `hash/fib.hpp::route_partition`.
- **What:** `partition = ((h >> 32) * P) >> 32`: no modulo, no floating point, identical in every language. The spec lists reference values, which hftkit's tests reproduce (P=4: 0,2,0,3,1,0,2,1).
- **Evidence:** `verified` against the spec values. **Caveats:** the map is fixed for the pipeline's lifetime.
- **FastMM:** n/a today (one engine thread); useful if the gateway ever shards. **Verdict:** adopt.

### Two-level occupancy bitmap
- **Where:** FastMM `L3Book`, CppTrader bitset scan, order-matching-engine, orderer-cpp ladder (`summary[i/4096]`); hftkit `ds/bit_ladder.hpp`.
- **What:** One bit per tick plus a summary bit per word, so `next_set`/`prev_set` touch a few words even across empty regions. orderer-cpp's fix for rescanning a whole ladder per cancel gave 4–24× on a 1M-operation workload (`claimed`).
- **FastMM:** has it. **Verdict:** adopt (done).

### Tombstone-free deletion by route counting
- **Where:** ultra-low-latency-orderbook (no licence, concept only).
- **What:** Double hashing where each slot counts how many probe paths cross it; a delete can restore a truly empty slot when the count is zero.
- **Caveats:** `remove` decrements counts before checking the key exists (latent corruption); prices 0 and `~0` are reserved markers; insert result ignored.
- **FastMM:** has backward-shift, which is simpler. **Verdict:** ignore.

### CLOCK (second-chance) cache
- **Where:** LruClockCache (MIT); hftkit `ds/clock_cache.hpp`.
- **What:** Replace list-based LRU with a reference bit per slot swept by a hand; a hit costs one lookup and one byte write. The original also uses a second "eviction hand" half a buffer ahead.
- **Evidence:** the key-0 bug is `verified` (reproduced on the unmodified header: a dirty write of key 0 is lost after other misses). hftkit's version uses explicit occupied/ref/dirty flags and is tested against an oracle plus a key-0 test.
- **Caveats:** original also erases dirty entries on `flush()`, polls a plain `bool` across threads, and runs user callbacks inside its critical section.
- **FastMM:** no general cache layer. **Verdict:** reimplement (done), do not adopt the code.

### Direct-mapped front cache
- **Where:** LruClockCache `DirectMappedCache`.
- **What:** Integer key masked into a slot, no hash map; useful where a miss is cheap.
- **Evidence:** `read`. **Verdict:** adapt only if a profile asks for it.

### Pre-faulted, huge-page tables
- **Where:** FastMM `HotArray`; lob-engine "prefault" constructor.
- **What:** Zero-fill at construction so no first-touch faults on the hot path; arrays ≥1 MiB use 2 MiB `MADV_HUGEPAGE`. lob-engine found its "zero syscalls on the matching path" claim false until it did this (516 minor faults in the first 1M events, then 0).
- **Evidence:** lob-engine `claimed`; hftkit `bench/pagefaults.hpp` demonstrates fault counting `verified` (first touch of 256 pages faults ~once per page; `MAP_POPULATE` does not).
- **FastMM:** has pre-faulting; lacks a test that counts faults. **Verdict:** adopt as test.

### Arena and pool allocation
- **Where:** order-matching-engine (`pmr::monotonic_buffer_resource` over 512 MB), ultra-low-latency-orderbook (`MemoryPool<Level>`), slick-orderbook (`ObjectPool` free-list blocks), orderer-cpp (zero-allocation test).
- **Caveats:** order-matching-engine's per-level `vector<Order>` still reallocates inside the arena and never reclaims until `reset()`; slick-orderbook's pool can grow during trading unless `reserve` is called. Both break a strict no-allocation rule.
- **FastMM:** fixed-capacity pools, better. **Verdict:** ignore the code, keep the allocation counter (section 6).

### Cached best-level pointers
- **Where:** CppTrader (`_best_bid`/`_best_ask`, "load-bearing for performance"), slick-orderbook `TopOfBook`, Limit-Order-Book edges.
- **Caveats:** every one of these has had a stale-cache bug: CppTrader/order-matching-engine "cached best ask pointing to empty level", slick-orderbook `deleteLevel`/`clear` leaving stale top of book. Add a regression test for each cache invalidation path.
- **Verdict:** adapt with tests.

### Sorted-vector price ladder
- **Where:** FastMM `L2Book` (best at the back), slick-orderbook (`std::flat_map`), lob-engine (SoA + AVX2 search), CppTrader optimized variant.
- **What:** Contiguous sorted levels; fast under ~100 levels, O(n) insert/delete beyond. Best-at-the-back makes near-touch inserts shift only a few entries.
- **Evidence:** lob-engine AVX2 vs scalar 42 vs 67 ns at 128 levels `claimed`. slick-orderbook cannot be best-at-back (`flat_map` is ascending).
- **FastMM:** has it, with a truncation flag the others lack. **Verdict:** ignore the others' versions.

### Packed signed price (bids positive, asks negative)
- **Where:** CppTrader "aggressive" benchmark variant.
- **What:** One 32-bit value encodes side and price. Untested in FastMM. **Verdict:** try on the hottest path only if a profile shows a need.
