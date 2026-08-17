# BDR v0.2+ Publication Gate

Status: project decision recorded 2026-08-17.

## Decision

No new public BDR software release will be published solely because the experimental engine reaches a performance or persistence milestone.

The current experimental line remains unpublished until it exposes a usable, documented application-facing API.

The published `v0.1.0` remains the immutable scientific/software baseline while the v0.2+ architecture evolves on an experimental branch.

## Minimum publication gate

Before the next software release is considered, the candidate must provide at least:

- stable database open/close lifecycle;
- `put`, `get`, and `delete` operations;
- explicit durability contracts (for example synchronous durable write and ticketed/group durability);
- `flush`/`sync` semantics;
- checkpoint support;
- reopen/recovery behavior;
- documented error model;
- versioned persistent format;
- reproducible API examples;
- API-level correctness and crash tests;
- compatibility/migration statement;
- benchmark methodology that distinguishes algorithmic, warm-index, and end-to-end comparisons.

## Release policy

Experimental benchmark versions (`Vxx`) are research checkpoints, not public releases.

When an API-bearing candidate satisfies correctness, durability, recovery, documentation, licensing and reproducibility gates, it may be promoted to beta/release-candidate evaluation. Publication still requires a final audit.

This policy does not modify the historical `v0.1.0` release or its artifacts.
