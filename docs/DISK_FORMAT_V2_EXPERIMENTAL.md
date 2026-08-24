# BDR Persistence Format v2 — Experimental candidate for software v0.2

Status: **experimental**. This document does not modify the published v0.1.0 disk-format contract and does not promise backward compatibility yet.

The purpose of this format is to give the concurrent/ticketed v0.2 candidate an explicit, self-identifying persistence envelope before snapshot/checkpoint integration.

## 1. Compatibility rule

The published v0.1.0 format remains unchanged.

A v0.2-format reader MUST NOT silently interpret a v0.1 WAL or an arbitrary file as a v0.2 WAL. Unknown magic, unsupported format versions and incompatible header sizes are hard errors.

Migration, if later implemented, must be explicit and separately tested.

## 2. WAL segment header

Byte order: little-endian for the C++ experimental implementation.

Every v2 WAL segment starts with a fixed header:

- magic: 4 ASCII bytes `BDW2`
- format_version: uint16, currently `2`
- header_size: uint16
- flags: uint32
- segment_id: uint64
- first_sequence: uint64
- reserved: uint64, must be zero when written
- header_crc32: uint32 over all preceding header bytes

Readers reject:

- wrong magic;
- unsupported version;
- unexpected header size;
- non-zero reserved field when strict mode is enabled;
- invalid header CRC.

## 3. WAL record

Immediately after the header, records are ordered by sequence.

Experimental record layout:

- key_len: uint32
- value_len: uint32
- sequence: uint64
- operation: uint8 (`1=PUT`, `2=DELETE`)
- record_flags: uint8
- reserved: uint16
- payload_crc32: uint32
- key bytes
- value bytes (zero bytes for DELETE)

`payload_crc32` covers operation metadata, key and value bytes, excluding the CRC field itself.

Sequence numbers must be strictly contiguous within replay scope. Recovery stops at the first torn tail. A complete record with invalid CRC is corruption and is not skipped.

## 4. Preallocation

Physical file size may exceed logical WAL length because v0.2 uses preallocated segments.

Therefore EOF is **not** the logical end of valid data.

The scanner terminates when the next bytes cannot form a valid record header/sequence. Zero-filled preallocated capacity is never accepted as a record.

A later manifest/checkpoint implementation should store the committed logical WAL boundary explicitly, reducing the need to infer the end by scanning.

## 5. Ticket durability

A ticket/sequence `T` may be acknowledged as durable only after the WAL writer has completed the configured durability barrier covering all records through `T`.

Two API contracts are expected:

- `put_sync(...)`: returns only after its own operation is durable;
- `submit(...) -> ticket` plus `wait(ticket)`: allows pipelining while preserving an explicit durable frontier.

The format does not encode client-side window size. The durable frontier is a runtime/manifest concern.

## 6. Snapshot candidate

The future v0.2 snapshot envelope will use a new magic distinct from the v0.1 `BDR2` snapshot. Proposed experimental magic: `BDR3`.

A snapshot must include at least:

- format version;
- snapshot sequence;
- encoder/index format version;
- record count;
- records or compact index/payload representation;
- whole-file checksum.

This section is intentionally incomplete until checkpoint experiments select the representation.

## 7. Checkpoint requirements

Before promotion, the implementation must prove:

1. WAL durable frontier is known;
2. snapshot is written to a temporary file;
3. snapshot file is synced;
4. atomic rename publishes the snapshot;
5. database directory is synced;
6. a new post-checkpoint WAL is durably created;
7. only then may obsolete WAL history be retired;
8. directory is synced after retirement.

Crash injection is required at every boundary.

## 8. Required compatibility tests

Before v0.2 release candidate status:

- valid header accepted;
- wrong magic rejected;
- future/unknown version rejected;
- header CRC corruption rejected;
- torn header rejected;
- truncated final record handled conservatively;
- middle-record corruption stops replay;
- preallocated zero tail is not interpreted as data;
- monotonic sequence enforced;
- v0.1 files are not silently interpreted as v0.2;
- snapshot/WAL sequence boundary prevents duplicate replay.

## 9. Status

This format is an engineering hypothesis under test. It becomes a release format only after checkpoint, reopen, corruption and SIGKILL tests pass and migration policy is documented.
