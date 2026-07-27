# C++ Matching Engine

A modern C++20 limit order book and matching engine built as a focused
systems-engineering project. The implementation will prioritize correctness,
deterministic price-time matching, clear ownership, and measured performance.

> **Project status:** Phase 6 adds bounded, deterministic multi-producer command
> ingestion around the single-threaded matching core.

## Prerequisites

- A C++20 compiler (Apple Clang, Clang, GCC, or MSVC)
- CMake 3.20 or newer
- Git (CMake fetches the pinned GoogleTest dependency during test configuration)

## Build and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/matching_engine_cli examples/sample_orders.csv
```

## Test

```sh
ctest --test-dir build --output-on-failure
```

CTest discovers the GoogleTest domain-model tests individually and reports any
validation or formatting failure by name.

## Benchmark

Benchmarks are opt-in so normal builds do not fetch Google Benchmark:

```sh
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DMATCHING_ENGINE_BUILD_BENCHMARKS=ON
cmake --build build-release --target matching_engine_benchmark --parallel
./build-release/matching_engine_benchmark
```

## Quality checks

```sh
# Formatting (requires clang-format)
cmake --build build --target format-check

# AddressSanitizer and UndefinedBehaviorSanitizer
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DMATCHING_ENGINE_ENABLE_ASAN_UBSAN=ON
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure

# ThreadSanitizer (use a separate build tree)
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug \
  -DMATCHING_ENGINE_ENABLE_TSAN=ON
cmake --build build-tsan --parallel
ctest --test-dir build-tsan --output-on-failure

# Static analysis (requires clang-tidy)
cmake -S . -B build-tidy -DCMAKE_BUILD_TYPE=Debug \
  -DMATCHING_ENGINE_ENABLE_CLANG_TIDY=ON
cmake --build build-tidy --parallel
```

## Domain representation

Prices use signed 64-bit integer ticks rather than floating point. Quantities
also use signed 64-bit integers so negative inputs can be rejected before they
reach the book. IDs and monotonic sequence numbers use unsigned 64-bit
integers. `Order` and `Trade` constructors reject invalid values so downstream
book logic can rely on their invariants. An order tracks both its original and
remaining quantity.

The order book stores bids in descending-price order and asks in
ascending-price order. Each price level owns a FIFO list of orders. A hash-table
index maps active order IDs to stable list iterators, supporting average
constant-time order lookup and list removal during cancellation.

An incoming limit order repeatedly executes against the best opposite-side
order while prices cross. Each trade executes at the resting order's price.
Orders at the same price execute in FIFO insertion order, and any incoming
remainder is added to the book.

## CSV command format

The CLI accepts one command per line. Fields are comma-separated, prices are
integer ticks, sides are uppercase, blank lines are ignored, and lines beginning
with `#` are comments.

```text
ADD,order_id,BUY|SELL,price_ticks,quantity
CANCEL,order_id
```

The application prints generated trades immediately, reports rejected lines to
standard error, continues after malformed input, and finishes with a readable
book snapshot. It exits with status `2` when one or more commands are rejected.

## Concurrency model

Producers submit commands with explicit, contiguous ingestion sequences to a
bounded blocking queue. A single worker reorders queued commands by that
sequence and is the only thread that mutates the matching engine. Condition
variables provide sleep-based coordination and bounded capacity provides
backpressure. Shutdown closes the queue, wakes blocked threads, resolves
pending futures, and joins the worker. This design is synchronized, not
lock-free.

## Planned capabilities

- Integer-priced limit orders with validated identifiers and quantities
- Deterministic price-time-priority matching and cancellation
- CSV-driven command-line demonstration
- Concurrent producer ingestion with a single matching thread
- Reproducible tests, sanitizer runs, benchmarks, and profiling evidence
- GitHub Actions continuous integration and detailed architecture notes

## License

This project is licensed under the [MIT License](LICENSE).
