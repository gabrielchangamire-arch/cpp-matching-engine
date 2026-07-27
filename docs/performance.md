# Performance Investigation

## Environment

Measurements were collected on 2026-07-27 with:

| Property | Value |
|---|---|
| Machine | Apple MacBook Pro (`Mac17,9`) |
| CPU | Apple M5 Pro, 15 logical CPUs reported |
| Memory | 24 GiB (25,769,803,776 bytes) |
| Operating system | macOS 26.5.2 (build 25F84) |
| Compiler | Apple Clang 21.0.0, arm64 target |
| Language/build | C++20, CMake Release |
| Benchmark framework | Google Benchmark 1.9.5, Release library |

Google Benchmark could not determine a meaningful CPU frequency on this Apple
Silicon system (it reported 24 MHz) and warned that thread affinity could not be
set. Those metadata limitations do not invalidate elapsed measurements, but
they reduce control over run-to-run noise.

## Methodology

The benchmark binary was built and executed with:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DMATCHING_ENGINE_BUILD_BENCHMARKS=ON
cmake --build build-release --target matching_engine_benchmark --parallel
./build-release/matching_engine_benchmark \
  --benchmark_min_time=0.1s \
  --benchmark_repetitions=5 \
  --benchmark_report_aggregates_only=true
```

Each result below is the median of five repetitions. Book/engine setup and
teardown use `PauseTiming`/`ResumeTiming`. Insertion and crossing benchmarks use
100 fixed iterations; cancellation uses 1,000. Other cases use Google
Benchmark calibration. The baseline and post-optimization runs used the same
source benchmark definitions and command.

`items/s` is computed by Google Benchmark from process CPU time. That is useful
for single-threaded cases. For multi-producer ingestion, wall time is also
reported because aggregate process CPU time has a different meaning when work
spans producer and worker threads.

## Baseline and post-optimization results

| Benchmark | Size | Baseline M items/s | Post M items/s | Change |
|---|---:|---:|---:|---:|
| Order insertion | 100 | 16.976 | 24.830 | +46.3% |
| Order insertion | 1,000 | 9.631 | 25.447 | +164.2% |
| Crossing match | 1 fill | 1.163 | 1.111 | -4.4% |
| Crossing match | 100 fills | 29.762 | 28.571 | -4.0% |
| Crossing match | 1,000 fills | 39.277 | 37.383 | -4.8% |
| Cancellation | 100 | 1.235 | 1.176 | -4.7% |
| Cancellation | 1,000 | 0.940 | 1.140 | +21.3% |
| Best bid + ask | 100 | 1,081.013 | 1,081.610 | +0.1% |
| Best bid + ask | 1,000 | 1,079.945 | 1,076.300 | -0.3% |
| Mixed workload | 1,000 operations | 20.534 | 25.847 | +25.9% |
| Single producer | 100 commands | 2.816 | 3.222 | +14.4% |
| Single producer | 1,000 commands | 2.618 | 4.175 | +59.5% |
| Four producers | 100 commands | 2.043 | 2.470 | +20.9% |
| Four producers | 1,000 commands | 3.003 | 8.689 | +189.4% |

The 1,000-command four-producer median was 611,405.9 ns baseline and 446,029.6
ns after the change in wall time. That corresponds to approximately 1.64 and
2.24 million commands/s, a 37.1% wall-throughput improvement. The much larger
CPU-time throughput delta in the table should not be read as end-to-end latency.

These baseline/post results describe the version 1.0 allocation experiment.
Version 1.1 subsequently changed concurrent-ingestion admission and sequence
bookkeeping; the single-threaded order-book and matching paths were unchanged.

Representative median CPU times were:

| Case | Baseline | Post |
|---|---:|---:|
| Insert batch, 256 operations, size 1,000 | 26,580 ns | 10,060 ns |
| Cross 1,000 resting orders | 25,460 ns | 26,750 ns |
| Mixed 1,000 operations | 48,699 ns | 38,688 ns |
| Single-producer 1,000 commands | 381,973 ns | 239,534 ns |

## Version 1.1 ingestion verification

The version 1.1 bounded sequence window and in-flight sequence cleanup were
rechecked on the same machine with the same Release build and five-repetition
median command. The run reported load averages of 5.46, 3.20, and 3.27, so it is
recorded as a current-code verification rather than compared directly with the
earlier optimization experiment.

| Case | Median wall time | Median CPU time | CPU-time throughput | Wall throughput |
|---|---:|---:|---:|---:|
| Single producer, 100 commands | 51,934 ns | 35,758 ns | 2.797 M/s | 1.926 M/s |
| Single producer, 1,000 commands | 346,311 ns | 265,620 ns | 3.765 M/s | 2.887 M/s |
| Four producers, 100 commands | 87,663 ns | 44,470 ns | 2.249 M/s | 1.141 M/s |
| Four producers, 1,000 commands | 511,243 ns | 180,067 ns | 5.553 M/s | 1.956 M/s |

## Profile evidence

The available system profiler was macOS `/usr/bin/sample`; Xcode Instruments'
command-line tooling was unavailable because the machine had Command Line Tools
rather than a full Xcode installation. A five-second, 1 ms interval sample of a
long-running baseline mixed workload collected 4,295 main-thread samples.

The call tree attributed 182 samples to
`MatchingEngine::submit_limit_order +368` in one path (and another 108 in a
second occurrence). Disassembly mapped offset `+368` to `operator new` reached
from this unconditional statement:

```cpp
trades.reserve(book_.order_count());
```

The vector reserved enough space for every active order on every submission,
including non-crossing orders that returned no trades. The allocation was
therefore avoidable on insertion-heavy and mixed paths.

## Optimization

The engine now reads the best opposite order first and reserves the trade
vector only if that order exists and crosses. The cross result is carried
through the fill loop rather than recomputing an extra branch solely for
reservation.

This is intentionally a small change: it preserves containers, public APIs,
matching rules, and asymptotic behavior. All 81 tests that existed during that
optimization phase, plus ASan/UBSan and TSan runs, passed after the modification.

Insertion and mixed cases improved materially, which is consistent with
removing an empty-result allocation. The optimization was not intended to
improve a crossing order that needs trade storage. Full-run crossing medians
regressed 4-5%, within a noisy fixed-iteration microbenchmark. A separate
ten-repetition check of the 1,000-fill case measured 39.025 M fills/s after the
change versus the 39.277 M baseline (about -0.6%), supporting the conclusion
that crossing performance was effectively unchanged in this experiment.

## Limitations

- Results describe one machine, compiler, standard library, allocator, and
  background-load state; they are not portable capacity promises.
- No CPU isolation, affinity pinning, fixed-frequency control, or thermal-state
  control was available.
- Fixed-iteration cases are intentionally short and visibly noisy.
- Google Benchmark process CPU time can understate or overstate intuitive
  end-to-end throughput for multiple threads; wall time is included where most
  relevant.
- The benchmark exercises an in-memory API, not parsing, network I/O,
  persistence, market-data publication, or risk checks.
- No p50/p95/p99 distribution is claimed. Five aggregate repetitions are not a
  correct tail-latency study.
- The profiler is a sampling tool. Symbol/disassembly mapping identifies a
  strong candidate, not an exact allocation cost model.
- Raw JSON and profile outputs are local generated artifacts and intentionally
  excluded from Git; the tables record their medians.
