from __future__ import annotations

import argparse
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


class CacheBackend:
    def __init__(self, cache: dict[str, bytes | None]) -> None:
        self.cache = cache
        self.get_calls = 0

    def get(self, key: str) -> bytes | None:
        self.get_calls += 1
        return self.cache.get(key)


def elapsed_ms(start_ns: int) -> float:
    return (time.perf_counter_ns() - start_ns) / 1_000_000


def build_fixture(count: int) -> tuple[AddressSpace, TemporalEventStore]:
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
) -> None:
    original_current = original_store.resolve("objeto 1", "cor", TemporalOperator.CURRENT)
    loaded_current = loaded_store.resolve("objeto 1", "cor", TemporalOperator.CURRENT)
    checks = {
        "current_state": original_current == loaded_current,
        "topology_metrics": original_addresses.metrics() == loaded_addresses.metrics(),
        "raw_count": len(original_addresses.iter_raw_memories()) == len(loaded_addresses.iter_raw_memories()),
        "event_count": len(original_store.iter_events()) == len(loaded_store.iter_events()),
        "transition_count": len(original_store.iter_transitions()) == len(loaded_store.iter_transitions()),
    }
    if not all(checks.values()):
        raise AssertionError(f"semantic parity failed at size {count}: {checks}")


def physical_keys_from_manifest(root_value: bytes) -> tuple[dict[str, object], list[str]]:
    manifest = json.loads(root_value.decode("utf-8"))
    if not isinstance(manifest, dict):
        raise AssertionError("root manifest must be a JSON object")
    keys: list[str] = []
    for field in ("node_keys", "raw_keys", "event_keys", "transition_keys"):
        keys.extend(str(item) for item in manifest.get(field, []))
    return manifest, keys


def run_size(count: int, library: Path, root: Path) -> dict[str, object]:
    addresses, store = build_fixture(count)

    capture = CaptureBackend()
    save_snapshot_to_backend(capture, addresses, store)
    operations = [Operation.put(key, value) for key, value in capture.puts]

    bdr_root = root / f"bdr-{count}"
    with AtomicBDR.open(bdr_root, library_path=library) as db:
        db.write_batch(operations, durability=DurabilityMode.BATCH_SYNC)

    with AtomicBDR.open(bdr_root, library_path=library) as db:
        started = time.perf_counter_ns()
        root_value = db.get(ROOT_KEY)
        root_get_ms = elapsed_ms(started)
        if root_value is None:
            raise AssertionError("missing frozen Memoria root manifest")

        started = time.perf_counter_ns()
        manifest, keys = physical_keys_from_manifest(root_value)
        manifest_decode_ms = elapsed_ms(started)

        started = time.perf_counter_ns()
        values = db.get_many(keys)
        bulk_get_ms = elapsed_ms(started)

        started = time.perf_counter_ns()
        cache: dict[str, bytes | None] = {ROOT_KEY: root_value}
        cache.update(zip(keys, values))
        cache_materialization_ms = elapsed_ms(started)

    missing = sum(1 for value in values if value is None)
    if missing:
        raise AssertionError(f"bulk extraction returned {missing} missing records at size {count}")

    cache_backend = CacheBackend(cache)
    started = time.perf_counter_ns()
    loaded_addresses, loaded_store = load_snapshot_from_backend(cache_backend)
    semantic_decode_restore_ms = elapsed_ms(started)
    assert_parity(count, addresses, store, loaded_addresses, loaded_store)

    extraction_ms = root_get_ms + bulk_get_ms
    staged_total_ms = extraction_ms + cache_materialization_ms + semantic_decode_restore_ms
    semantic_share = semantic_decode_restore_ms / staged_total_ms if staged_total_ms else 0.0
    extraction_share = extraction_ms / staged_total_ms if staged_total_ms else 0.0

    return {
        "observations": count,
        "physical_records": len(capture.puts),
        "bulk_keys": len(keys),
        "manifest_counts": {
            "nodes": len(manifest.get("node_keys", [])),
            "raw": len(manifest.get("raw_keys", [])),
            "events": len(manifest.get("event_keys", [])),
            "transitions": len(manifest.get("transition_keys", [])),
        },
        "timing_ms": {
            "root_get": root_get_ms,
            "manifest_decode_for_key_discovery": manifest_decode_ms,
            "bulk_get": bulk_get_ms,
            "bdr_extraction_total": extraction_ms,
            "python_cache_materialization": cache_materialization_ms,
            "memoria_semantic_decode_restore": semantic_decode_restore_ms,
            "staged_total": staged_total_ms,
        },
        "shares": {
            "bdr_extraction": extraction_share,
            "memoria_semantic_decode_restore": semantic_share,
        },
        "cache_get_calls_during_codec": cache_backend.get_calls,
        "parity": True,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True, type=Path)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="bdr-v123-") as temp_dir:
        root = Path(temp_dir)
        samples = [run_size(size, args.library, root) for size in SIZES]

    report = {
        "benchmark": "v123_frozen_memoria_decode_decomposition",
        "memoria_validated_commit": MEMORIA_VALIDATED_COMMIT,
        "sizes": list(SIZES),
        "contract": {
            "frozen_memoria_codec_unmodified": True,
            "bdr_format_unmodified": True,
            "cache_backend_is_measurement_only": True,
            "performance_thresholds": False,
        },
        "samples": samples,
    }
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
