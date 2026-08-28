# Remaining Android C ABI requirements

The Android cross-build is green, but Memoria.ia must not consume the basic per-key ABI as its final persistence contract.

Remaining requirements:

- additive atomic C ABI backed by `bdr::AtomicDatabase`;
- one logical Memoria.ia memory/turn persisted as one atomic batch;
- binary-safe batch put/delete operations;
- `exists`/contains query;
- explicit ABI/version identifier callable at runtime;
- sync/durable-sequence observability;
- integrity/recovery check appropriate to the BDW4 atomic store;
- interrupted/torn-write recovery test through the C boundary;
- Android arm64-v8a symbol/artifact CI for the additive atomic surface;
- exact ABI/version consumed by Memoria.ia documented.

The existing `bdr_c_*` basic ABI remains source-compatible and is not redefined to fake atomicity.
