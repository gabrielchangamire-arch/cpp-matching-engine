# C++ Matching Engine

A modern C++20 limit order book and matching engine built as a focused
systems-engineering project. The implementation will prioritize correctness,
deterministic price-time matching, clear ownership, and measured performance.

> **Project status:** Phase 2 provides validated order and trade domain models.
> Order-book and matching functionality will be added in subsequent, tested
> phases.

## Prerequisites

- A C++20 compiler (Apple Clang, Clang, GCC, or MSVC)
- CMake 3.20 or newer
- Git (CMake fetches the pinned GoogleTest dependency during test configuration)

## Build and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/matching_engine_cli
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

## Planned capabilities

- Integer-priced limit orders with validated identifiers and quantities
- Deterministic price-time-priority matching and cancellation
- CSV-driven command-line demonstration
- Concurrent producer ingestion with a single matching thread
- Reproducible tests, sanitizer runs, benchmarks, and profiling evidence
- GitHub Actions continuous integration and detailed architecture notes

## License

This project is licensed under the [MIT License](LICENSE).
