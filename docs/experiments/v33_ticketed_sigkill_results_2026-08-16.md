# V33 — Concurrent ticketed SIGKILL recovery

Date: 2026-08-16/17
Branch: `experiment/hierarchical-resolutive-addressing`
Status: experimental; `main` and `v0.1.0` remain unchanged.

## Configuration

- 8 producer threads
- producer durability window: 128
- physical WAL batch maximum: 512
- preallocated WAL
- dedicated ordered WAL writer
- monotonically increasing ticket/sequence
- CRC per record
- process terminated with real `SIGKILL`
- recovery uses WAL only

The workload target was 2,000,000 records so that the process would still be active at each crash point.

## Results

| SIGKILL delay | valid records recovered | last_seq | bad | tail_stop |
|---:|---:|---:|---:|---:|
| 10 ms | 23,952 | 23,952 | 0 | 1 |
| 20 ms | 39,473 | 39,473 | 0 | 1 |
| 50 ms | 52,736 | 52,736 | 0 | 1 |
| 100 ms | 130,304 | 130,304 | 0 | 1 |

For every crash:

```text
good == last_seq
bad == 0
```

The scanner stopped at the first non-record area of the preallocated WAL and did not accept zero-filled capacity as valid data.

## Interpretation

This validates process-crash recovery for the concurrent ticketed WAL configuration used in the current high-throughput candidate. It demonstrates recovery of a strict valid prefix after `SIGKILL` on this runner.

It is not a power-loss test. `SIGKILL` leaves the operating system and storage stack alive; separate fault-injection or power-loss/emulation work would be required for stronger claims.

## Current state

The candidate architecture now has evidence for:

- high concurrent durable throughput in V32;
- explicit durability contracts (strict and ticketed/group);
- recovery from WAL only;
- CRC/sequence validation;
- truncation/corruption handling;
- real process crash recovery under concurrency.

## Remaining publication gates

Before considering v0.2 a publication candidate:

1. larger-scale runs (>= 1M records) against the selected market baselines;
2. mixed read/write workloads and hot/skew distributions;
3. memory/RSS measurement under the concurrent candidate;
4. versioned persistent format/magic for the new WAL layout;
5. checkpoint/snapshot integration and reopen tests with the new format;
6. repeatability on more than one machine/storage environment if available.
