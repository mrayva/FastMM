# hftkit

Small, header-only, dependency-free C++20 building blocks for low-latency market-data and
matching code. Every component is a clean-room reimplementation of an *idea* seen in one of a
set of open-source trading repositories, with tests that pin the behaviour (see
`docs/PROVENANCE.md`). Nothing is copied.

| Header | What it gives you |
|---|---|
| `hash/fib.hpp` | Fibonacci hashing (top bits), multiply-shift partition routing with spec reference values |
| `check/cluster.hpp` | Longest cyclic run of occupied slots, to test a hash function structurally rather than statistically |
| `ds/open_index.hpp` | Fixed-capacity `uint64 -> uint32` open-addressing map, linear probing, backward-shift erase (no tombstones) |
| `ds/bit_ladder.hpp` | Two-level occupancy bitmap with `next_set` / `prev_set` |
| `ds/clock_cache.hpp` | CLOCK (second-chance) cache with explicit occupied/ref/dirty flags; key 0 is an ordinary key |
| `journal/crc32c.hpp`, `journal/record_log.hpp` | Per-record CRC-32C framing; scan distinguishes clean / torn tail / zero-filled tail / corrupt |
| `journal/ack_gate.hpp` | Holds acks until a durable watermark passes them (journaled vs durable are different) |
| `net/sizing.hpp` | Size a reorder / retransmit window as time (slots / rate) against round-trip time |
| `check/conservation.hpp` | Quantity conservation ledger: `submitted == 2*filled + resting + cancelled + rejected + expired` |
| `check/ref_book.hpp` | Slow, obviously-correct reference order book (`std::map`) for differential testing |
| `check/alloc_counter.hpp` | Opt-in global `operator new` counter to assert "no allocation on the hot path" |
| `bench/pagefaults.hpp` | Minor/major page-fault counters around a scope |
| `bench/open_loop.hpp` | Open-loop latency measurement from the *intended* send time (avoids coordinated omission) |
| `vectors/itch5.hpp`, `vectors/moldudp64.hpp` | Hand-typed ITCH 5.0 / MoldUDP64 wire vectors with expected field values |

## Build and test

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build && ctest --test-dir build
cmake -S . -B build-asan -DHFTKIT_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug   # ASan + UBSan
```

Use from another project: `add_subdirectory(hftkit)` and link `hftkit::hftkit`, or just add
`include/` to your include path.

## Status and limits

* Single-threaded data structures; no concurrency primitives are provided.
* `test_open_loop` and `test_pagefaults` depend on OS timing / paging behaviour and use wide
  margins; they are the tests most likely to need adjusting on an unusual host.
* Wire vectors were typed from the specifications. The ITCH Add vector was additionally run
  through an independent decoder (FastMM's); other vector types are checked against the spec
  offsets only.
* `RefBook` implements one explicit set of matching rules (documented in the header) which
  is not claimed to match any particular venue.
