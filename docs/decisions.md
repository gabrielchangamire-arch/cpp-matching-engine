# Architecture Decisions

These short records capture choices that materially shape the implementation.

## ADR-001: Represent prices as integer ticks

**Status:** Accepted

**Decision:** `Price` is a signed 64-bit integer. The caller defines what one
tick means.

**Why:** Binary floating point cannot exactly represent many decimal prices and
can make comparisons at price boundaries surprising. Integer comparison is
exact, cheap, and easy to test. A signed type lets constructors reject negative
input before it reaches the book.

**Tradeoff:** The type does not encode currency, scale, instrument, or tick-size
rules. A production API would likely use a stronger value type plus instrument
metadata.

## ADR-002: Use ordered maps of FIFO lists for the book

**Status:** Accepted

**Decision:** Store bid/ask price levels in `std::map`, ordered best first, and
orders at a price in `std::list` FIFO order.

**Alternatives considered:** A heap makes best price cheap but arbitrary
cancellation and level aggregation awkward. A flat vector improves locality but
can make insertion and iterator stability expensive. A custom intrusive tree or
pool could reduce allocations but would add substantial ownership complexity.

**Why:** The selected containers directly express price ordering and time
ordering, provide stable iterators, and offer predictable logarithmic
price-level operations. They are an explainable correctness-first baseline.

**Tradeoff:** Tree and list nodes allocate separately and have weaker cache
locality than flat or pooled structures.

## ADR-003: Serialize matching on one worker

**Status:** Accepted

**Decision:** Permit concurrent producers, but mutate a book from exactly one
matching thread.

**Why:** Matching one command can touch multiple orders and price levels. A
single writer preserves deterministic sequencing and keeps the core free of
fine-grained locks. It also resembles an event-loop/sharded-engine design: scale
can come from independent books rather than concurrent mutation of one book.

**Tradeoff:** One book cannot use several cores for matching one sequence. Queue,
future, and thread-handoff overhead make the concurrent facade slower than a
direct call at small workloads.

## ADR-004: Index cancellation by active order ID

**Status:** Accepted

**Decision:** Maintain an `std::unordered_map` from order ID to side, price, and
stable list iterator.

**Alternatives considered:** Scanning every level is simpler but `O(N)`.
Indexing only side and price still requires a linear scan within the level.

**Why:** A known active order can normally be found and erased in average
constant time. `std::list` iterators remain valid when other list elements are
inserted or removed.

**Tradeoff:** Every mutation must keep the book and index consistent, and the
hash table adds memory and allocations. Erasing an emptied map level remains
`O(log P)`.

## ADR-005: Fetch pinned test and benchmark dependencies with CMake

**Status:** Accepted

**Decision:** Use `FetchContent` with GoogleTest `v1.17.0` and Google Benchmark
`v1.9.5`; benchmark fetching is opt-in.

**Why:** A fresh developer or CI machine gets reproducible framework versions
without a system package layout assumption. Normal consumers do not download
Google Benchmark unless requested.

**Tradeoff:** The first configure needs Git and network access. Vendoring would
support offline builds but increase repository size and update burden. A package
manager could integrate larger dependency graphs but is unnecessary here.

## ADR-006: Benchmark medians before and after one profiled change

**Status:** Accepted

**Decision:** Use Release builds, Google Benchmark timed regions, five
repetitions, median aggregates, fixed iterations for setup-heavy microbenchmarks,
and identical baseline/post commands. Profile the baseline before changing code.

**Why:** Multiple repetitions reduce the influence of a single noisy run, and
paused setup avoids measuring construction/destruction that is not part of the
named operation. A before/after profile prevents speculative optimization.

**Tradeoff:** A laptop is not an isolated performance lab. Frequency metadata,
affinity control, background load, allocator effects, and process CPU accounting
for multiple threads limit generalization. Results are directional evidence,
not service-level latency claims.
