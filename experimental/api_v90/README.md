# V90 — Native market benchmark for the consolidated API

This gate compares the consolidated BDR API (V86/V87 line) against native SQLite, LMDB, LevelDB and RocksDB under durability-aware workloads.

## Method

- same key/value payloads for every engine;
- writers: 1, 4, 8, 16;
- three repetitions per configuration;
- report median throughput;
- BDR uses ticketed writes and waits at the selected durability boundary;
- competitors use their synchronous/durable write options;
- results are rejected if final-state verification fails.

This is an experimental gate. It does not modify the published v0.1.0 baseline and is not a release artifact.
