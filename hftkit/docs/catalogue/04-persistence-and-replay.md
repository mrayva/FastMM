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
