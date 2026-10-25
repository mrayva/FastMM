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

### Shared-memory segment header as a compatibility contract
- **Where:** raikiri `libs/shm/segment.hpp` (LGPL-3.0, concept only).
- **What:** a segment starts with a magic number, an ABI version, an FNV layout hash over the channel directory, creator pid, a generation number and a heartbeat. Channels are bump-allocated by the owner, each with a 16-byte name and size; `open_channel<T>` refuses a size mismatch, and a client of a different ABI or layout is refused instead of misreading. Thirty-two reader slots take a lease with one compare-exchange (claim count and pid packed together); the owner counts as dead if its pid is gone or its heartbeat is over 200 ms stale; a generation change means "drop cursors and reconcile".
- **Evidence:** `read`. Compare with `ShmRing::open`, and with low-latency-event-engine's attach checks (section 4).
- **Verdict:** adopt as a design checklist.

### Race-free seqlock (payload as relaxed atomic words)
- **Where:** raikiri `SeqlockCell` and `BroadcastRing` (concept only).
- **What:** the payload is stored as an array of relaxed `atomic<uint64_t>` words between version stores, so the seqlock has no data race and needs no ThreadSanitizer fallback (FastMM's `Seqlocked<T>` falls back to a spinlock under TSan). The broadcast ring gives every slot its own version, so a lapped reader gets an exact `Overrun` and the count of items missed; its cursor then jumps to the head.
- **Evidence:** `read`. The code requires `sizeof(T) % 8 == 0`.
- **Caveats (found by reading, not run):** a reader loops forever if the writer dies mid-write (odd version); the 32-bit version wraps through 0, which readers treat as "never written", after 2^31 writes (about 24 days at 1,000 writes/s).
- **FastMM:** has `Seqlocked<T>` and rings; the per-slot-version broadcast ring is the reliable-overrun answer that turboq's multicast lacks. **Verdict:** adapt.

### Wrap-around via a payload offset in each message header
- **Where:** turboq `SPSCMessageQueue` (MIT).
- **What:** a variable-length byte ring where each message is a 64-byte header line plus a cache-line-aligned payload. If the payload does not fit before the end, the header is still written at the tail but its `payloadOffset` is 0 and the payload goes at the start of the ring. The consumer follows the offset, so the pointer is the wrap marker, no padding record is needed, and every payload is one contiguous span (zero copy).
- **Caveats:** a minimum of 128 bytes per message (an 8-byte message costs two cache lines); a strict `<` is needed so equal positions never mean both full and empty.
- **FastMM:** `MsgRing` writes a padding record at the wrap instead. **Verdict:** note as an alternative; the measured round trip is in the noise (section 7).

### MPSC ring with reservation and publication as separate steps
- **Where:** turboq `MPSCMessageQueue`.
- **What:** fixed-size power-of-two slots; a CAS on `producerPos` reserves a slot, the producer writes it, then sets a `committed` flag kept in a separate slot-header array (flags do not share lines with payloads); the consumer clears the flag and then advances `consumerPos`.
- **Caveats:** a producer that dies between the CAS and the commit stalls the consumer forever; there is no timeout or recovery. `prepare` throws if the message exceeds the slot size.
- **Verdict:** adapt only with a stale-slot policy.

### Reserved user region in the queue file
- **Where:** turboq `Options::reserveSpace`: bytes right after the header, cache-line aligned, zero-filled at creation, recorded in the header and checked on open. New header fields went into former padding so old files still open. **Verdict:** note.

### L3 window as a ring (FastMM 0.9.0)
- **Where:** upstream FastMM `l3_book.hpp` (MIT).
- **What:** tick `t` lives in slot `t mod window`, wherever the window starts, so moving the window moves no slot; a recentre only moves the levels that change sides between the window and the overflow store. A summary bitmap marks non-zero words (64 ticks per word, 4,096 per summary word). `clear()` costs what the book holds, not its capacity.
- **Evidence:** `verified`; see section 2.

### L2 snapshot load in O(levels); timer wheel generations
- **Where:** upstream FastMM 0.9.0 (changelog and commits, `read`): a new level in a full side moves only the levels worse than it; a `TimerId` carries its slot generation so a stale id cannot cancel a reused slot; the wheel keeps eight slots for the engine's own timers so a strategy cannot starve a flatten; a callback that cancels the next timer no longer crashes the wheel.
- **Verdict:** note; the wheel bugs are worth a regression test in any timer wheel.
