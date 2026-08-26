# BDR v1 internal candidate

This directory marks the internal convergence line toward v1.0. It is not a release staging area and must not create public tags, release candidates, or DOI publications before v1.0.

## Candidate path

The currently preferred internal configuration is:

- CompactIndex backend;
- streaming BDR3 checkpoint encoding;
- unchanged BDR3 snapshot format;
- unchanged BDW3 WAL format;
- existing public Database API/ABI kept stable unless a later v1 review explicitly approves a change.

## Mandatory fallback

Until final v1 readiness is proven, the previous ResolutiveIndex backend remains a compile-time fallback for differential testing and regression isolation.

## Promotion invariants

A change may remain on this branch only if it preserves:

1. exact key/value semantics against an external oracle;
2. BDR3/BDW3 compatibility;
3. torn-final-WAL handling and CRC rejection;
4. crash recovery across checkpoint atomicity boundaries;
5. concurrency correctness for 1/4/8/16 writers;
6. deterministic durable sequence recovery;
7. no resurrection of deleted keys;
8. no material regression in high-cardinality memory or throughput without documented justification.

## Current validated evidence inherited from v0.3

- 50M mutation scale gate: PASS;
- compact contract parity: PASS;
- compact persistence parity: PASS;
- compact crash recovery and concurrency: PASS;
- compact + streaming combined lifecycle: PASS;
- paired 5M compact + streaming: PASS;
- 12 checkpoint crash-boundary failpoints on baseline and compact streaming: PASS;
- paired 5M compact + streaming median RSS approximately 24.3% below baseline streaming;
- BDR3 disk footprint remains byte-compatible for the validated streaming path.

## Next engineering step

Remove the temporary v0.3 shim/generator mechanism in small, testable steps. The internal candidate gate must remain green after each step, and the fallback backend must remain available until the final v1 readiness review.
