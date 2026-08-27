# V103 — Commit Boundary Evidence

Status: PASS

Branch: `develop/v1.1.0`

## Objective

Validate the durability/acknowledgement boundary for the candidate BDW4 atomic batch path before integrating it into the public `Database` API.

## Contract validated

- a strict partial append never recovers a partial logical batch;
- a torn tail is repaired to the last complete batch boundary;
- a complete append before `fdatasync` is not acknowledged;
- after `fdatasync` but before ACK, recovery contains the complete batch even though the caller may not know the commit outcome;
- a successful return (ACK) occurs only after `fdatasync` and implies recoverability;
- a durable committed prefix survives a torn following batch unchanged.

This intentionally follows standard transactional uncertainty semantics: **ACK implies durable commit; absence of ACK does not imply absence of commit** when a crash occurs after the durability boundary but before acknowledgement.

## Memoria.ia representative logical batch

The test batch contains multiple physical keys corresponding to payload, node, occurrence and metadata records. At every injected pre-commit truncation point it recovers either all records or none, never a subset.

## CI evidence

GitHub Actions workflow: `V103 Commit Boundary`

Result: PASS — configure, build, durability/ACK boundary test.

## Scope boundary

V103 remains isolated under `experimental/api_v103`. It does not modify the frozen BDR v1.0.0 engine. The next gate is a candidate `write_batch` API built on this validated commit boundary.
