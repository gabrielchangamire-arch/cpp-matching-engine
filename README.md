# C++ Matching Engine

A modern C++20 limit order book and matching engine built as a focused
systems-engineering project. The implementation will prioritize correctness,
deterministic price-time matching, clear ownership, and measured performance.

> **Project status:** Phase 1 establishes the repository and a minimal,
> warning-clean executable. Order and matching functionality will be added in
> subsequent, tested phases.

## Prerequisites

- A C++20 compiler (Apple Clang, Clang, GCC, or MSVC)
- CMake 3.20 or newer

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

The current smoke test verifies that the initial executable starts and exits
successfully. GoogleTest unit and integration coverage will be introduced with
the domain model.

## Planned capabilities

- Integer-priced limit orders with validated identifiers and quantities
- Deterministic price-time-priority matching and cancellation
- CSV-driven command-line demonstration
- Concurrent producer ingestion with a single matching thread
- Reproducible tests, sanitizer runs, benchmarks, and profiling evidence
- GitHub Actions continuous integration and detailed architecture notes

## License

This project is licensed under the [MIT License](LICENSE).
