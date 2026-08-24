# V45 — BDW3 protected WAL framing

Status: experimental. Not a published disk-format commitment.

## Motivation

BDW2 stores record lengths inside the record and protects the complete record with a CRC located at the end. A corruption of `key_len` or `value_len` can make a complete record look like an incomplete/torn tail before the parser reaches that CRC. This creates an ambiguity between legitimate tail truncation and corruption of an already-written record header.

## Candidate change

BDW3 adds protected framing before payload processing:

`total_len | seq | op | key_len | value_len | header_crc | key | value | record_crc`

The segment header uses magic `BDW3` and version `3`.

Validation order:

1. Validate segment magic/version/header CRC.
2. Read and bounds-check `total_len`.
3. Read the complete bounded frame or classify it as a torn tail.
4. Validate the fixed-header CRC before trusting key/value lengths.
5. Validate operation code and internal length consistency.
6. Validate the complete record CRC.
7. Only then apply the operation.

## Intended invariant

- Corrupted framing/header bytes in a complete record must be rejected explicitly.
- A physically incomplete final frame may be treated as a torn tail and ignored conservatively.
- No untrusted length may trigger an unbounded allocation.

## Historical preservation

BDW2 remains preserved in the experimental history. BDW3 is a candidate replacement for the unreleased v0.2 line only; it does not modify the published v0.1.0 format.
