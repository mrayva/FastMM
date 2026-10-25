# 2. Order books and matching semantics

### Windowed tick book with bitmap and overflow store
- **Where:** FastMM `L3Book` (MIT).
- **What:** Prices near the market index a tick window (price→level is one subtraction); a bitmap per side finds the next level; outliers go to a sorted overflow store; the window recentres on the midpoint. Orders sit in a pool with an intrusive FIFO per level and an open-addressing index.
- **Evidence:** `verified`: book state matched CppTrader's `MarketManager` on all 2,475 symbols after 1,000,000 messages of `sample.itch`, 56,090 live orders, 0 book errors.
- **Caveats:** `verified` weakness in the version I first analysed (f76e871): recentre cost was linear in the window. During the 12:00:00 mass cancel AAPL recentred 48 times at the default 2^16 window (~225 µs each; ~11 µs at 2^12, ~60 µs at 2^14, ~800 µs at 2^18), which made the last 63k messages 15–25× slower. Setting the tick to $0.01 fixed AAPL but gave 22 book errors across the top 100 symbols (sub-penny orders).
- **Fixed in 0.9.0 (`verified`):** the window is now a ring and a recentre waits until the touch has traded outside it (below). Same file, same harness: whole-slice cost 722–758 → 64–82 ns/message, recentres 9,829 → 2,911, and per-symbol best bid, best ask and order count are byte-identical to the old build after 1,000,000 and after all 1,563,071 messages, with 0 book errors.
- **Verdict:** FastMM's own; the mitigation I proposed (hysteresis and a cheaper shift) is what 0.9.0 did.

### Flat direct-indexed book
- **Where:** order-matching-engine, orderer-cpp, kaspar `TachBook` (flat `uint[262144]` array by tick).
- **Caveats:** fixed price range (order-matching-engine 0..99999, silently dropping others). kaspar's constructor assert exists because an NQ ladder of 117,509 ticks once exceeded the 102,400-tick array and two books silently published nothing: a good example of a startup assert replacing a silent failure.
- **FastMM:** window plus overflow is better. **Verdict:** adopt the startup-assert lesson.

### Tombstoned FIFO per level
- **Where:** order-matching-engine: `pmr::vector<Order>` with `headIndex`, cancel marks inactive via `idToLocation`.
- **Caveats:** matcher must skip inactive entries; memory is not reclaimed until the level empties. **Verdict:** ignore (intrusive lists are cleaner).

### AVL level trees with stop books
- **Where:** Limit-Order-Book, CppTrader reference manager.
- **Evidence:** Limit-Order-Book `claimed` ~713 ns/request average; per-request instrumentation attributes ~440 ns per extra executed trade and ~2.5 µs per AVL rebalance.
- **Caveats:** rebalance calls a recursive height function, so it is not O(log M). **Verdict:** ignore the structure, keep the attribution idea (section 7).

### Stop and stop-limit cascade
- **Where:** Limit-Order-Book, CppTrader (also trailing stops).
- **What:** After any trade, trigger stops whose price the touch has crossed, in price order; a triggered stop-limit becomes a limit order that re-enters matching.
- **Caveats:** Limit-Order-Book silently drops a stop-market on an empty book.
- **FastMM:** the simulated exchange has no stops. **Verdict:** adapt if the simulator must model stops; use the repos' tests as cases.

### In-flight mitigation on modify
- **Where:** CppTrader `MitigateOrder`.
- **What:** After a modify, leaves = `new_qty − executed_qty` (0 if overfilled), so a size change racing fills cannot overfill.
- **Caveats:** modify is delete-mutate-re-add (loses priority); `ModifyOrder` had a crash fix (#42) and asserts on missing ids in debug.
- **Verdict:** adopt the rule where modify semantics are needed.

### All-or-none matching chains; iceberg; hidden volume
- **Where:** CppTrader (`CalculateMatchingChain`, per-level Total/Hidden/Visible volume).
- **Caveats:** the AON area has a history of fixes (#28, #31). Absent from FastMM's simulator. **Verdict:** adapt only if the simulated venue needs those order types.

### Modify rules (priority)
- **Where:** lob-engine and hftkit `check/ref_book.hpp`.
- **What:** quantity decrease keeps time priority; increase (or price change) loses it; 0 is a cancel. matching-engine-benchmark's workload only contains increase-modifies, so decrease is untested there.
- **Evidence:** hftkit's `RefBook` encodes these rules and its tests `verified` them; they are not claimed to match any venue.

### Self-trade prevention counter, FOK atomicity
- **Where:** lob-engine; hftkit `RefBook` (FOK rejected whole, nothing partially applied).
- **Verdict:** adopt as test cases.

### Queue-position accounting
- **Where:** kaspar `OB` (per-tick FIFO gives exact position), FastMM `queue_model.hpp` (modelled), roq-algo `QueuePositionSimple` (unfinished: `ahead` never reduced, `match_resting_orders_2` has a TODO and an assert that would fire).
- **FastMM:** has the best model-based version (section 5). **Verdict:** ignore roq-algo's; FastMM's functions are the reference.

### Cross handling
- **Where:** kaspar `UNCROSS_BOOK`/`OB::add` cancel contra orders the aggressor crossed; slick-orderbook and others do not handle crossed books.
- **Verdict:** adopt as a defined policy when consuming a feed that may transiently cross.

### Deferred recentre as a cost-benefit rule
- **Where:** upstream FastMM 0.9.0 `note_touch_out()`.
- **What:** when a side's best level is outside the window, the overflow store serves that touch until it has handled about as many operations as the move would cost; then the window moves onto the midpoint of the two touches (or onto the touch of a one-sided book). Touches further apart than the window keep the window where it is, so a stub-only side does not pull it back and forth. A failed recentre is counted (`recentre_failures()`) and retried; the book stays correct, only slower. `recentre_levels_moved()` counts the work. Documented costs: in-window add/execute/cancel/replace O(1); overflow-store operations a binary search plus a memmove.
- **Evidence:** `verified` end to end on `sample.itch` (above); the 2,911 remaining recentres are the real cost of the mass cancel.
- **Verdict:** the reference for any windowed book.

### Sorted-vector ladder, best at the front
- **Where:** raikiri `book::Ladder` (concept only): a `std::vector<Level>` kept sorted so the touch is element 0, with `upsert`, `remove`, `truncate(depth)` and `drop_beyond(worst)`; `snapshot()` returns nothing for a one-sided or untrusted book. `vector::insert` may reallocate unless `reserve` is called.
- **FastMM:** keeps the best at the back, so near-touch inserts shift few entries. **Verdict:** ignore the layout, keep "an untrusted book publishes as nothing".
