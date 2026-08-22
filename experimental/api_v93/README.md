# BDR API v93 — External Usage Package (Experimental)

This directory documents how an external user would consume the consolidated BDR API candidate. It is **not a release** and does not change `main` or the published `v0.1.0` baseline.

## Supported experimental stack

- Core: `experimental/api_v86`
- C ABI: `experimental/api_v87`
- Python binding: `experimental/api_v88`
- On-disk formats: BDR3 snapshot + BDW3 WAL
- Platform target for this candidate: Linux
- C++ standard: C++17
- C ABI version: 1

## Durability model

`put()` and `delete()` are asynchronous submission calls. They return a ticket after the mutation has been accepted by the in-memory index and submission queue.

A ticket becomes durable only after:

```text
wait(ticket)
```

or an equivalent `sync()` that covers the ticket.

The synchronous helpers:

```text
put_sync()
delete_sync()
```

return only after the corresponding mutation has crossed the durable WAL boundary.

This distinction is part of the public contract and must not be weakened in a release candidate.

## C++ quickstart

See `quickstart_cpp.cpp`.

Core API shape:

```cpp
auto db = bdr::Database::open("./example-db");
auto ticket = db->put("key", "value");
db->wait(ticket);
auto value = db->get("key");
db->checkpoint();
db->close();
```

## C quickstart

See `quickstart_c.c`.

The C ABI is binary-safe. Keys and values are passed as pointer + length; embedded NUL bytes are supported.

Applications should initialize options with:

```c
bdr_options opt;
bdr_options_init(&opt);
```

and should check every returned `bdr_status`.

## Python quickstart

See `quickstart_python.py`.

The Python package uses only the C ABI v1 and should be installable as a self-contained wheel produced by the experimental build pipeline.

## Process model

A BDR directory is single-process-open by design in the current candidate. Multiple threads within one process are supported. A second process attempting to open the same database directory must be rejected while the first process holds the process lock.

## Recovery model

- WAL frames use BDW3 framing and integrity checks.
- Snapshots use BDR3.
- A torn final WAL tail may be truncated back to the last fully valid frame.
- Corruption inside already-written frames must be rejected, not silently skipped.
- Snapshot recovery is streaming in the hardened candidate to avoid unnecessary full-snapshot materialization.

## Error model

The C ABI exposes stable status classes including invalid argument, not found, already open, I/O error, closed handle, incompatible ABI, and internal error. `bdr_last_error()` provides a thread-local diagnostic string for the most recent failing C ABI call.

Writer I/O errors must propagate to callers; they must not call `abort()` or leave `wait()` blocked indefinitely.

## Not yet a release

This package is only a documentation/integration gate. Publication remains blocked until the unified candidate audit and external-consumption tests produce verifiable passing evidence.
