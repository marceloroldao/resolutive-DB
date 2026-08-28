# Resolutive-DB Android C ABI contract

Current baseline: `bdr/c_api.h`, cross-built with Android NDK 27.2 for `arm64-v8a` / Android 26.

The basic ABI (`open`, `put`, `get`, `delete`, `sync`, `checkpoint`, `close`) is a compatibility layer over `bdr::Database`.

For Memoria.ia Android persistence, **do not model one logical memory as several independent `put` calls**. The required durable contract remains:

`1 logical memory = 1 atomic BDR batch = all committed or all absent after recovery`.

Therefore the mobile integration must use the additive atomic C ABI tracked in Issue #23, backed by `bdr::AtomicDatabase` / BDW4. The atomic surface must include batch write, read/exists, sync, version/ABI reporting and recovery/integrity evidence. The existing basic ABI remains source-compatible.

Architecture boundary:

`OFF.IA/Kotlin -> Memoria.ia mobile ABI -> BDR atomic C ABI -> BDW4 persistence`

OFF.IA must not inspect BDR files or substitute SQLite/Room for this path.
