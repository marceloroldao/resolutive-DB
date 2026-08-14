# BDR V13 — persistent WAL handle and real durability boundary

Date: 2026-08-14
Branch: `experiment/v0.2-cpp-encoder`

## Why this test exists

The earlier C++ persistence prototype reopened an `std::ofstream` for every append and called `flush()` after each record. Two separate questions were therefore mixed:

1. overhead from reopening/closing the WAL for every operation;
2. actual durable-commit cost.

`std::ofstream::flush()` drains the C++ stream buffer but is not equivalent to POSIX `fsync()`/`fdatasync()`. Therefore an earlier comparison between BDR `flush()` and SQLite `COMMIT` with durable settings cannot be described as durability-parity.

## Experiment A — persistent file handle

A local controlled implementation kept the WAL handle open for the database lifetime and still flushed each operation. In an initial run, write latency fell from approximately 1.52 us/op to 0.55 us/op (~2.77x). Repeated runs showed substantial filesystem/environment variance, so this figure is treated as directional evidence only.

Conclusion: eliminating per-operation `open/close` is a valid implementation optimization, but its exact gain must be measured on the target system.

## Experiment B — real durability

The reproducible C++ benchmark in `benchmarks/bdr_v13_persistent_handle_fsync.cpp` uses a persistent POSIX file descriptor and calls `fdatasync()` once per operation. Seven repetitions of 2,000 operations were measured in the available environment.

Observed medians in this run:

- BDR buffered/no `fdatasync`: ~0.621 us/op
- BDR `fdatasync` per operation: ~600.7 us/op
- SQLite via Python binding, `journal_mode=WAL`, `synchronous=FULL`, commit per operation: ~385.5 us/op

SQLite repetitions were measured separately using the same operation count. Python-driver overhead remains present, so the comparison is still not a perfect C-vs-C microbenchmark; nevertheless, SQLite was faster despite that overhead in this environment.

## Corrected interpretation

The earlier statement that the C++ BDR persistence layer was approximately 8x faster than SQLite under the *same durability guarantee* is **not supported** by the current code because the BDR side used `flush()` rather than `fsync`/`fdatasync`.

The defensible conclusions are now:

- the append-only WAL design is functionally sound under the corruption tests performed;
- maintaining a persistent file handle removes avoidable overhead;
- true per-operation durable sync dominates write latency;
- in the present environment, SQLite FULL durable commits outperformed the simple BDR `fdatasync`-per-record implementation;
- group commit remains the correct next optimization for durable throughput.

## Recovery behavior

A strict recovery variant was also tested locally. Mid-file corruption caused an explicit `RecoveryError` / checksum mismatch instead of merely incrementing a `bad` counter. This should be preferred for the production API, while diagnostic counters may remain available as structured error metadata.

## Next tests

1. persistent descriptor + group commit at 8/16/32/64/128 operations;
2. `fdatasync` vs `fsync` characterization;
3. C/C++ SQLite benchmark if development headers are available;
4. p50/p95/p99 durable-commit latency;
5. batched WAL record encoding to reduce syscall count;
6. crash injection at group-commit boundaries;
7. integrate strict recovery semantics into the persistent engine API.

This result is preserved as a methodological correction and negative/neutral result, in accordance with the project's reproducibility policy.
