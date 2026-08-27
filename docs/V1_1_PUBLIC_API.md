# BDR v1.1 Public Atomic API

Status: internal release candidate; unreleased.

BDR v1.1 preserves the v1.0 `bdr::Database` source surface and adds an opt-in atomic logical-write API through `bdr/atomic_database.hpp`. Both are distributed through the same installed CMake target, `bdr::bdr`.

## Compatibility rule

Existing v1.0 code may continue to include only:

```cpp
#include <bdr/database.hpp>
```

and use `bdr::Database` unchanged. v1.1 does not reinterpret BDR3 or BDW3 persistence and does not require callers to migrate to the atomic API.

Applications that require all-or-nothing persistence for one logical object may additionally include:

```cpp
#include <bdr/atomic_database.hpp>
```

## Atomic database

Open the v1.1 atomic surface over an existing v1 directory:

```cpp
auto db = bdr::AtomicDatabase::open("data");
```

If the second path is omitted, new atomic writes use `data/atomic.bdw4`. Existing BDR3/BDW3 state is read first and remains unchanged; BDW4 is layered as an explicitly versioned side-by-side WAL.

## Durability modes

```cpp
enum class DurabilityMode : std::uint8_t {
    Async = 0,
    BatchSync = 1,
    PerOperationSync = 2,
};
```

- `Async`: complete BDW4 frame is appended and becomes visible to the current process, but the API does not claim a durable boundary until `sync()`.
- `BatchSync`: one complete logical batch is appended and durably flushed before a durable result is returned.
- `PerOperationSync`: compatibility mode for exactly one operation. Multi-operation calls are rejected rather than pretending that per-operation durability is an atomic multi-operation transaction.

## Atomic batch

A batch uses the existing public `bdr::Operation` and `bdr::OperationType` types:

```cpp
std::vector<bdr::Operation> ops = {
    {bdr::OperationType::Put, "memory/payload", payload},
    {bdr::OperationType::Put, "memory/node/1", node},
    {bdr::OperationType::Put, "memory/meta", metadata},
};

auto result = db->write_batch(std::move(ops), bdr::DurabilityMode::BatchSync);
```

`BatchResult` contains one batch sequence, the number of operations and whether that call crossed a durability boundary.

## Bulk helpers

`put_many()` and `erase_many()` are convenience paths built on the same atomic batch primitive. Their default durability is `BatchSync`.

## Sequence semantics

- `last_sequence()` is the latest complete batch visible to the current process.
- `durable_sequence()` is the latest sequence for which this process has crossed a known durability boundary.
- `sync()` promotes the current complete async prefix to a durable prefix.
- After reopen, every complete recoverable BDW4 frame is part of the persisted recovered prefix; torn final frames are discarded to the last valid boundary.

## Crash-recovery contract

BDW4 frames carry an explicit format version, batch sequence, operation count and complete-frame CRC. Recovery validates the entire frame and every operation before applying any mutation. A torn/incomplete final batch therefore appears as none of the batch; a complete valid frame appears as all of the batch.

This property is validated by V109 across every truncation position of a representative multi-operation batch, including put/delete mixtures and side-by-side legacy BDR3/BDW3 data.

## Memoria.ia usage

The intended Memoria.ia pattern is one atomic batch per logical memory, for example payload + nodes + occurrences + metadata. This avoids treating a loop of separately persisted keys as a transaction.

V112 validates a representative workload of 512 logical memories × 24 physical records and compares one durability boundary per logical memory against the v1.0 write cadence. Performance figures are retained in `CHANGELOG.md` as workload-specific regression evidence.

## Non-goals

v1.1 does not introduce SQL-style interactive transactions, MVCC, distributed transactions, Windows/macOS portability abstraction, or checkpoint telemetry. Those remain separate roadmap work.
