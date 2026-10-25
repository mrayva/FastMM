# 4. Persistence and replay

### Journal-before-apply with hash-verified replay
- **Where:** FastMM `.fmj` journal, `fastmm-replay --verify` (MIT).
- **What:** every consumed event is recorded before it is acted on; replay compares the SHA-256 of the outbound order stream and prints the first differing message. Operator actions (parameter updates, flatten) are journaled inputs. Blocks are CRC32C-checked, ≤1 MiB, never split a message; clock stored as int32 ns deltas; header embeds effective config.
- **Evidence:** `read`. **FastMM:** has it. This is the strongest idea in the whole set.

### Flight recorder
- **Where:** submicro-execution-engine `system_determinism.hpp` (proprietary, concept only): journal non-deterministic inputs (packets, `rdtsc`, time checks) so replay returns the same values. FastMM does this with the whole engine driven from the journal. **Verdict:** has it.

### Per-record CRC-32C and tail repair
- **Where:** orderer-cpp `JOURNAL.md` (v2 adds CRC-32C per record; truncate a torn final record; cut zero-filled tails; delete a segment whose header never arrived); hftkit `journal/record_log.hpp`.
- **Evidence:** hftkit `verified`: CRC-32C known answer for "123456789" is 0xE3069283; cutting the file at every byte offset and zero-filling 1..80 trailing bytes are classified as clean prefix, torn tail or zero-filled tail; interior damage is `Corrupt`.
- **FastMM:** has per-block CRC and discards a partial tail; the zero-fill case (file extended with zeros after an interrupted write) is not known to be tested. **Verdict:** adopt as test.

### Two watermarks: journaled and durable
- **Where:** orderer-cpp; hftkit `journal/ack_gate.hpp`.
- **What:** `journaled_iseq` (written) lets the engine proceed; `durable_iseq` (fsynced) releases acks. A publish means "sequenced", not "durable". The fsync policy (every N records, every T ms, never) changes latency and loss on power failure, never journal content.
- **Evidence:** hftkit gate refuses to mark durable past journaled and releases in order, `verified`.
- **FastMM:** one ring-to-disk path. **Verdict:** adopt as design if acks ever need a durability promise.

### Sequenced control operations
- **Where:** orderer-cpp: barrier, snapshot and shutdown travel in ingress order, so a snapshot is a consistent cut. FastMM's journaled `Control` records are comparable. **Verdict:** compare on partial failure.

### Length-last commit in a shared mmap file
- **Where:** revoq (Apache-2.0).
- **What:** frame header 32 bytes, stride rounded to 64; the writer reserves a slot, fills it, then release-stores `length` last; readers acquire-load `length` and ignore the frame until it is nonzero. No shared head/tail; the same files are IPC and journal, so replay is opening a cursor from the start. Segments rotate with a `SegmentEnd` sentinel; optional warmer roles pre-map the next segment so the hot thread takes no page fault.
- **Evidence:** `read`; published p50 ~106 ns, p99 ~138 ns (32 B, tuned host) `claimed`, not reproduced.
- **Caveats:** no CRC in the frame header; recovery after a mid-frame writer crash is under-documented; durability is page cache only; unbounded file growth and no backpressure; `msg_type` 0 is "no data".
- **FastMM:** `MsgRing`/`ShmRing` are SPSC with shared indices. A length-last file could simplify gateway fan-out to several strategies at the cost of backpressure. **Verdict:** adopt as an experiment.

### Validated shared-memory ring ABI
- **Where:** low-latency-event-engine: 64-byte preamble (magic `LLE1`, version, sizes, slot count/stride, random nonzero `instance_id`, state), separate cursor lines; attach checks magic, version, zeroed reserved bytes, exact segment size, overflow-checked arithmetic; creator releases `READY`, attacher acquires; explicit `CLOSED` so queued events drain; the consumer reads the closed state *before* the write cursor so a last publication is not missed. Acquire/release slots give in-place reads (no copy-out, unlike FastMM's `MsgRing`).
- **Evidence:** `read`; the repo reports 131 tests under ASan/UBSan/TSan `claimed`.
- **FastMM:** compare against `ShmRing::open`. **Verdict:** adopt as review checklist.

### Second-process replay byte-diff
- **Where:** lob-engine: record a binary trace and the fills, replay in a fresh process, byte-diff the fills. It caught uninitialised struct padding that made two recordings of identical input differ, and a misaligned reference from packing the on-disk struct.
- **Verdict:** adopt as test; zero-initialise any record struct written to a journal.

### Snapshot plus journal recovery at any partition count
- **Where:** orderer-cpp: checkpoints rotate every partition onto a new segment at a clean cut and remove covered ones; recovery restores cores at any P. Single book per symbol, no cross-partition ordering. **Verdict:** read for design.

### Mmap segment journal with a committed-bytes handoff
- **Where:** raikiri `libs/journal` (LGPL-3.0, concept only); same family as revoq and orderer-cpp.
- **What:** 256 MiB preallocated segments; the hot append is a `memcpy` plus a release-store of `committed_bytes` in the file header, and readers acquire-load it. Frames are `len | schema | version | seq | ts | payload | crc32c`, 8-byte aligned, payload ≤64 KiB. A flock on `<prefix>.lock` allows one writer. Recovery walks frames from the header, stops at the first bad CRC or unexpected sequence number, and re-seals the previous segment. A blank newest segment (interrupted rotation) is adopted. Retention keeps N sealed segments. `flush()` is `fdatasync`/`F_FULLFSYNC` on a cold thread and is skipped if nothing was appended.
- **Evidence:** `read`; its kill -9 test accepts the one whole frame a kill can leave past the watermark.
- **Caveats (by reading):** recovery treats the first bad frame as the tail, so interior corruption silently drops every later record and the writer overwrites it (hftkit's `scan_records` reports that case as `Corrupt`); there is no durable watermark for acks; `open_after` scans linearly from the first segment.
- **Verdict:** adapt with the fixes named above.

### Why FastMM stopped writing its journal through an mmap
- **Where:** upstream FastMM 0.9.0 commit `0fdfcd7` and `docs: what a timer tick and a TLB shootdown cost a spinning core` (MIT).
- **What:** the journal writer used to copy blocks into a shared mapping and remap every 64 MiB extent. Kernel write-back of a shared mapping's dirty pages, and each `munmap`, flush the TLB of every CPU the process runs on, so a pinned spinning thread took an interrupt about every 2 MB. Their measurements, from the commit messages: 4 GiB at 270 MB/s gave 2,141 TLB shootdowns on the writer's core with the mapping and none with `pwrite`; a session journalling 1.6 MB/s took 29,700 shootdowns in two minutes on its engine core, about 120 with `pwrite`. A thread spinning on a core without `nohz_full` stalled 5–10 µs about a thousand times a second, and TLB shootdowns still reached isolated cores (1–2 µs each). Extents are still reserved with `posix_fallocate`, so a full disk fails an extent ahead; `journal_sync` values `async` and `fdatasync` keep their meaning.
- **Evidence:** `claimed` (their numbers, from commit messages; not reproduced here). The mechanism is standard.
- **Relevance:** raikiri, revoq and turboq all use shared mappings for their on-disk or shared-memory files. For a pinned hot thread the writer-side cost is worth measuring; for a shared-memory ring between processes the mapping is the design, so it only applies to the file-backed journal.
- **Verdict:** adopt as a measurement to run before choosing mmap for a journal.

### Journal records a session's outside inputs, checkpoints included
- **Where:** upstream FastMM 0.8.0–0.9.0 (changelog, `read`): `[engine] checkpoint_s` journals positions and open orders for any strategy; an `AccountFlow` event records money moved in or out of an account outside trades; a journal part after the first carries the engine clock its records continue from (journal format 4, not readable by 0.8). A backtest can start at a checkpoint (`[backtest] start_at`), take the session's own latencies (`latency_from_journal`) and its flows (`balances_from_journal`), and compare configurations from the same moment (`[ab] assignment = "branch"`).
- **Verdict:** the design to follow for "replay a live incident from a checkpoint".

### Single-file queue with tag, size and minimum-size checks
- **Where:** turboq: on open the tag, capacity and reserved size are validated (`TagMismatch`, `SizeMismatch`), and a file smaller than the header is rejected before mapping (`BufferTooSmall`), because mapping it would SIGBUS. Single-producer and single-consumer are enforced with open-file-description byte-range locks (`F_OFD_SETLK`), which also work for two handles in one process, plus in-process flags for the same-handle case.
- **Verdict:** adopt as a checklist next to raikiri's `WriterLease` (flock) and low-latency-event-engine's attach checks.
