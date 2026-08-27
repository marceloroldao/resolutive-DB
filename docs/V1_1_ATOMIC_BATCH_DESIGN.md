# BDR v1.1 — Atomic Batch Design

Status: design baseline before implementation

## Problem

Memoria.ia persists one logical memory as multiple physical records (payload, unique nodes, occurrences and metadata). A loop of individual `put()` calls followed by `sync()` is durable for the synced prefix but is not atomic: a process failure during the logical write may leave a partially applied memory.

v1.1 must add a transaction-like write boundary with all-or-nothing recovery semantics while preserving all existing v1.0 calls.

## Proposed public model

```cpp
enum class DurabilityMode : std::uint8_t {
    Async = 0,
    BatchSync = 1,
    PerOperationSync = 2,
};

struct BatchResult {
    std::uint64_t sequence = 0;
    std::size_t operations = 0;
};

BatchResult write_batch(std::vector<Operation> operations,
                        DurabilityMode durability = DurabilityMode::BatchSync);
```

Convenience APIs may be layered on top:

```cpp
BatchResult put_many(...);
BatchResult erase_many(...);
```

The atomic primitive is `write_batch`; bulk helpers must not create a weaker durability model.

## Required semantics

- Empty batches are rejected or explicitly documented as no-op; no ambiguous durable sequence is emitted.
- A batch has one commit identity/sequence visible to callers.
- The index is made visible atomically with respect to the batch boundary.
- Recovery after a crash yields either the complete committed batch or none of it.
- A torn/incomplete WAL batch is discarded as a unit.
- Existing v1.0 single-operation WAL recovery remains supported.
- Existing BDR3 snapshots and BDW3 WALs remain readable.

## WAL strategy

Do not silently reinterpret BDW3 frames. If atomic framing cannot be represented without changing the meaning accepted by v1.0 readers, introduce an explicitly versioned WAL format (candidate name `BDW4`).

Preferred migration behavior:

1. v1.1 opens existing BDR3 + BDW3 data;
2. after recovery, new writes may roll to the new WAL version;
3. the new reader understands both BDW3 and the new atomic format;
4. old v1.0 readers are not claimed to read new atomic WALs;
5. snapshot migration remains explicit and documented.

A batch frame should carry enough information to validate the entire logical unit before mutating the recovered index. Candidate fields include batch sequence, operation count, encoded operations, frame length and CRC over the complete batch.

## Writer ordering

Atomicity requires more than a final `sync()` call. The writer must ensure that no subset of a logical batch is treated as independently committed during recovery.

The implementation should preserve current asynchronous writer ordering for legacy operations while adding a distinct pending-batch representation. Mixing legacy operations and batches must have deterministic total ordering.

## Crash matrix

At minimum test deterministic failures:

- before batch WAL append;
- during batch header write;
- during operation payload write;
- before batch CRC/trailer completion;
- after complete append but before durable flush;
- during durable flush boundary where injectable;
- after durable flush but before in-memory acknowledgement;
- during checkpoint with committed batches present;
- reopen after a torn final batch;
- reopen with legacy BDW3 followed by new-format WAL.

For each point, expected recovered state must be specified as all-or-none for the logical batch.

## Memoria.ia acceptance gate

A representative logical memory batch must include multiple keys for payload, nodes, occurrences and metadata. The test must prove:

- no partial memory after forced crash;
- complete reconstruction after acknowledged commit;
- batch path improves or preserves the direct-native integration performance envelope;
- complete Memoria.ia regression remains green.

## Non-goals for v1.1

- general multi-version concurrency control;
- long-lived interactive SQL-style transactions;
- cross-database distributed transactions;
- changing the BDR3 snapshot contract unless required by evidence;
- Windows/macOS platform abstraction (planned for v1.2.0).
