from __future__ import annotations

import argparse
from dataclasses import asdict
import json
from pathlib import Path
import tempfile
import time

from bdr.atomic import AtomicBDR, DurabilityMode, Operation
from memoria_resolutiva.topological_bdr_persistence import (
    load_snapshot_from_backend,
    save_snapshot_to_backend,
)
from memoria_resolutiva.topological_memory import AddressSpace, TemporalEventStore, TemporalOperator
from memoria_resolutiva.topological_persistence import load_snapshot, save_snapshot


SIZES = (24, 240, 1200)
MEMORIA_VALIDATED_COMMIT = "99a1585d497b98f0fc6f360ec8f39e6771452827"
ROOT_KEY = "memoria.topology.v1/root"


class CaptureBackend:
    def __init__(self) -> None:
        self.puts: list[tuple[str, bytes]] = []

    def write_batch(self, puts: list[tuple[str, bytes]]) -> int:
        self.puts = list(puts)
        return 1

    def get(self, key: str) -> bytes | None:
        raise RuntimeError("capture backend is write-only")


class AtomicBackendAdapter:
    def __init__(self, db: AtomicBDR) -> None:
        self.db = db

    def write_batch(self, puts: list[tuple[str, bytes]]) -> int:
        result = self.db.write_batch(
            [Operation.put(key, value) for key, value in puts],
            durability=DurabilityMode.BATCH_SYNC,
        )
        return result.sequence

    def get(self, key: str) -> bytes | None:
        return self.db.get(key)


class BulkPrefetchBackendAdapter:
    """Benchmark-only adapter that keeps Memoria semantics outside BDR.

    The frozen codec still issues get(key) calls. On the root-manifest read this
    adapter uses the manifest's generic physical key lists to fetch all remaining
    records in one BDR get_many call, then serves the codec from a Python cache.
    """

    def __init__(self, db: AtomicBDR) -> None:
        self.db = db
        self.cache: dict[str, bytes | None] = {}
        self.bulk_get_ms = 0.0
        self.bulk_keys = 0

    def get(self, key: str) -> bytes | None:
        if key in self.cache:
            return self.cache[key]
        value = self.db.get(key)
        self.cache[key] = value
        if key != ROOT_KEY or value is None:
            return value

        manifest = json.loads(value.decode("utf-8"))
        keys: list[str] = []
        for field in ("node_keys", "raw_keys", "event_keys", "transition_keys"):
            keys.extend(str(item) for item in manifest.get(field, []))
        self.bulk_keys = len(keys)
        started = time.perf_counter_ns()
        values = self.db.get_many(keys)
        self.bulk_get_ms = elapsed_ms(started)
        self.cache.update(zip(keys, values))
        return value


def elapsed_ms(start_ns: int) -> float:
    return (time.perf_counter_ns() - start_ns) / 1_000_000


def disk_bytes(path: Path) -> int:
    if not path.exists():
        return 0
    if path.is_file():
        return path.stat().st_size
    return sum(item.stat().st_size for item in path.rglob("*") if item.is_file())


def build_fixture(count: int) -> tuple[AddressSpace, TemporalEventStore]:
    """Exact fixture shape from Memoria.ia frozen topological_multisize_benchmark.py."""
    addresses = AddressSpace()
    store = TemporalEventStore(addresses)
    colors = ("azul", "preta", "branca", "vermelha")
    states = ("ligado", "espera", "ativo", "inativo")

    for i in range(count):
        subject = f"objeto {i % 24}"
        attribute = "cor" if i % 3 else "estado"
        value = colors[(i // 24) % len(colors)] if attribute == "cor" else states[(i // 24) % len(states)]
        rare = " xqz91" if i == 0 else ""
        raw = f"registro de {subject} com {attribute} de {value} evento {i}{rare}."
        ingestion = addresses.ingest_text(raw)
        store.observe_state(subject, attribute, value, raw_memory_address=ingestion.raw_memory_address)
    return addresses, store


def assert_parity(
    count: int,
    original_addresses: AddressSpace,
    original_store: TemporalEventStore,
    loaded_addresses: AddressSpace,
    loaded_store: TemporalEventStore,
) -> dict[str, bool]:
    original_current = original_store.resolve("objeto 1", "cor", TemporalOperator.CURRENT)
    loaded_current = loaded_store.resolve("objeto 1", "cor", TemporalOperator.CURRENT)
    parity = {
        "current_state": original_current == loaded_current,
        "topology_metrics": original_addresses.metrics() == loaded_addresses.metrics(),
        "raw_count": len(original_addresses.iter_raw_memories()) == len(loaded_addresses.iter_raw_memories()),
        "event_count": len(original_store.iter_events()) == len(loaded_store.iter_events()),
        "transition_count": len(original_store.iter_transitions()) == len(loaded_store.iter_transitions()),
    }
    if not all(parity.values()):
        raise AssertionError(f"frozen Memoria semantic parity failed at size {count}: {parity}")
    return parity


def run_size(count: int, library: Path, root: Path) -> dict[str, object]:
    addresses, store = build_fixture(count)
    metrics = addresses.metrics()

    capture = CaptureBackend()
    started = time.perf_counter_ns()
    stats = save_snapshot_to_backend(capture, addresses, store)
    serialization_ms = elapsed_ms(started)
    payload_bytes = sum(len(key.encode("utf-8")) + len(value) for key, value in capture.puts)

    operations_started = time.perf_counter_ns()
    operations = [Operation.put(key, value) for key, value in capture.puts]
    operation_build_ms = elapsed_ms(operations_started)

    bdr_root = root / f"bdr-{count}"
    db = AtomicBDR.open(bdr_root, library_path=library)
    started = time.perf_counter_ns()
    write_result = db.write_batch(operations, durability=DurabilityMode.BATCH_SYNC)
    native_batchsync_ms = elapsed_ms(started)
    write_diag = db.diagnostics()
    db.close()
    bdr_bytes = disk_bytes(bdr_root)

    started = time.perf_counter_ns()
    db = AtomicBDR.open(bdr_root, library_path=library)
    python_open_ms = elapsed_ms(started)
    reopen_diag = db.diagnostics()

    started = time.perf_counter_ns()
    loaded_addresses, loaded_store = load_snapshot_from_backend(AtomicBackendAdapter(db))
    semantic_get_rebuild_ms = elapsed_ms(started)
    single_get_parity = assert_parity(count, addresses, store, loaded_addresses, loaded_store)
    db.close()

    db = AtomicBDR.open(bdr_root, library_path=library)
    bulk_adapter = BulkPrefetchBackendAdapter(db)
    started = time.perf_counter_ns()
    bulk_addresses, bulk_store = load_snapshot_from_backend(bulk_adapter)
    bulk_prefetch_rebuild_ms = elapsed_ms(started)
    bulk_parity = assert_parity(count, addresses, store, bulk_addresses, bulk_store)
    db.close()

    sqlite_path = root / f"sqlite-{count}.sqlite3"
    started = time.perf_counter_ns()
    sqlite_stats = save_snapshot(sqlite_path, addresses, store)
    sqlite_save_ms = elapsed_ms(started)
    started = time.perf_counter_ns()
    sqlite_addresses, sqlite_store = load_snapshot(sqlite_path)
    sqlite_load_ms = elapsed_ms(started)
    sqlite_current = sqlite_store.resolve("objeto 1", "cor", TemporalOperator.CURRENT)
    original_current = store.resolve("objeto 1", "cor", TemporalOperator.CURRENT)
    if sqlite_current != original_current or sqlite_addresses.metrics() != addresses.metrics():
        raise AssertionError(f"SQLite oracle parity failed at size {count}")

    accounted_open_us = reopen_diag.legacy_load_us + reopen_diag.wal_replay_us
    python_open_us = int(round(python_open_ms * 1000.0))

    return {
        "observations": count,
        "nodes": len(addresses.iter_nodes()),
        "raw_memories": len(addresses.iter_raw_memories()),
        "events": len(store.iter_events()),
        "transitions": len(store.iter_transitions()),
        "node_reuse_ratio": metrics["node_reuse_ratio"],
        "physical_records": stats.physical_records,
        "serialized_key_value_bytes": payload_bytes,
        "serialization_ms": serialization_ms,
        "python_operation_build_ms": operation_build_ms,
        "bdr": {
            "batchsync_ms": native_batchsync_ms,
            "disk_bytes": bdr_bytes,
            "sequence": write_result.sequence,
            "write_diagnostics": asdict(write_diag),
            "cold_open_ms": python_open_ms,
            "cold_open_unaccounted_us": max(0, python_open_us - accounted_open_us),
            "reopen_diagnostics": asdict(reopen_diag),
            "semantic_single_get_rebuild_ms": semantic_get_rebuild_ms,
            "semantic_bulk_prefetch_rebuild_ms": bulk_prefetch_rebuild_ms,
            "bulk_get_ms": bulk_adapter.bulk_get_ms,
            "bulk_keys": bulk_adapter.bulk_keys,
            "bulk_rebuild_speedup": (
                semantic_get_rebuild_ms / bulk_prefetch_rebuild_ms
                if bulk_prefetch_rebuild_ms > 0
                else None
            ),
        },
        "sqlite_oracle": {
            "save_ms": sqlite_save_ms,
            "load_ms": sqlite_load_ms,
            "disk_bytes": disk_bytes(sqlite_path),
            "physical_events": sqlite_stats.events,
        },
        "parity": {
            "single_get": single_get_parity,
            "bulk_prefetch": bulk_parity,
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True, type=Path)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="bdr-v122-") as temp_dir:
        root = Path(temp_dir)
        samples = [run_size(size, args.library, root) for size in SIZES]

    report = {
        "benchmark": "v122_frozen_memoria_topological",
        "memoria_validated_commit": MEMORIA_VALIDATED_COMMIT,
        "sizes": list(SIZES),
        "contract": {
            "fixture_matches_frozen_multisize_shape": True,
            "codec_imported_from_memoria": True,
            "one_snapshot_one_atomic_batch": True,
            "sqlite_is_oracle_only": True,
            "performance_thresholds": False,
            "negative_results_must_be_recorded": True,
            "bulk_prefetch_is_benchmark_adapter_not_bdr_semantics": True,
        },
        "samples": samples,
    }
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
