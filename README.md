# C++ Matching Engine

A modern C++20 limit order book and matching engine built as a focused
systems-engineering project. The implementation will prioritize correctness,
deterministic price-time matching, clear ownership, and measured performance.

> **Project status:** Phase 5 adds a CSV-driven command-line application to the
> deterministic matching library.

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

## Planned capabilities

- Integer-priced limit orders with validated identifiers and quantities
- Deterministic price-time-priority matching and cancellation
- CSV-driven command-line demonstration
- Concurrent producer ingestion with a single matching thread
- Reproducible tests, sanitizer runs, benchmarks, and profiling evidence
- GitHub Actions continuous integration and detailed architecture notes

## License

This project is licensed under the [MIT License](LICENSE).
