from __future__ import annotations

import argparse
import ctypes
import json
from pathlib import Path
import tempfile
import time

from bdr.atomic import AtomicBDR, DurabilityMode, Operation, _CBuffer, _CKey, _raise_status, _to_bytes
from memoria_resolutiva.topological_bdr_persistence import save_snapshot_to_backend
from memoria_resolutiva.topological_memory import AddressSpace, TemporalEventStore


SIZES = (24, 240, 1200)
MEMORIA_VALIDATED_COMMIT = "99a1585d497b98f0fc6f360ec8f39e6771452827"


class CaptureBackend:
    def __init__(self) -> None:
        self.puts: list[tuple[str, bytes]] = []

    def write_batch(self, puts: list[tuple[str, bytes]]) -> int:
        self.puts = list(puts)
        return 1

    def get(self, key: str) -> bytes | None:
        raise RuntimeError("capture backend is write-only")


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


def run_raw_bulk(db: AtomicBDR, keys: list[str]) -> dict[str, float | int]:
    started = time.perf_counter_ns()
    c_keys = (_CKey * len(keys))()
    keepalive = []
    for index, key in enumerate(keys):
        raw = _to_bytes(key, field="key")
        buf = ctypes.create_string_buffer(raw, len(raw))
        keepalive.append(buf)
        c_keys[index].data = ctypes.cast(buf, ctypes.c_void_p)
        c_keys[index].size = len(raw)
    out_values = (_CBuffer * len(keys))()
    found = (ctypes.c_int * len(keys))()
    key_marshalling_ms = elapsed_ms(started)

    started = time.perf_counter_ns()
    _raise_status(
        db._lib.bdr_atomic_c_get_many(db._handle, c_keys, len(keys), out_values, found),
        "get_many",
    )
    native_call_ms = elapsed_ms(started)

    values: list[bytes | None] = []
    started = time.perf_counter_ns()
    for index in range(len(keys)):
        if not found[index]:
            values.append(None)
        elif out_values[index].size == 0:
            values.append(b"")
        else:
            values.append(ctypes.string_at(out_values[index].data, out_values[index].size))
    python_materialize_ms = elapsed_ms(started)

    started = time.perf_counter_ns()
    for item in out_values:
        db._lib.bdr_atomic_c_free_buffer(item)
    python_free_ms = elapsed_ms(started)

    total_value_bytes = sum(len(value) for value in values if value is not None)
    return {
        "key_marshalling_ms": key_marshalling_ms,
        "native_call_ms": native_call_ms,
        "python_materialize_ms": python_materialize_ms,
        "python_free_ms": python_free_ms,
        "measured_total_ms": key_marshalling_ms + native_call_ms + python_materialize_ms + python_free_ms,
        "total_value_bytes": total_value_bytes,
        "found": sum(int(x) for x in found),
    }


def run_size(count: int, library: Path, root: Path) -> dict[str, object]:
    addresses, store = build_fixture(count)
    capture = CaptureBackend()
    save_snapshot_to_backend(capture, addresses, store)
    operations = [Operation.put(key, value) for key, value in capture.puts]
    keys = [key for key, _ in capture.puts]

    bdr_root = root / f"bdr-{count}"
    with AtomicBDR.open(bdr_root, library_path=library) as db:
        db.write_batch(operations, durability=DurabilityMode.BATCH_SYNC)

    with AtomicBDR.open(bdr_root, library_path=library) as db:
        raw = run_raw_bulk(db, keys)
        started = time.perf_counter_ns()
        public_values = db.get_many(keys)
        public_get_many_ms = elapsed_ms(started)

    if len(public_values) != len(keys) or any(value is None for value in public_values):
        raise AssertionError(f"bulk read parity failed at size {count}")
    expected_bytes = sum(len(value) for _, value in capture.puts)
    if raw["total_value_bytes"] != expected_bytes:
        raise AssertionError(f"raw bulk byte count mismatch at size {count}")

    measured = float(raw["measured_total_ms"])
    native_share = float(raw["native_call_ms"]) / measured if measured else 0.0
    python_post_share = (
        float(raw["python_materialize_ms"]) + float(raw["python_free_ms"])
    ) / measured if measured else 0.0

    return {
        "observations": count,
        "physical_records": len(keys),
        "serialized_value_bytes": expected_bytes,
        "raw_decomposition": raw,
        "public_get_many_ms": public_get_many_ms,
        "shares": {
            "native_call": native_share,
            "python_materialize_and_free": python_post_share,
        },
        "parity": True,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True, type=Path)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="bdr-v124-") as temp_dir:
        root = Path(temp_dir)
        samples = [run_size(size, args.library, root) for size in SIZES]

    print(json.dumps({
        "benchmark": "v124_get_many_buffer_decomposition",
        "memoria_validated_commit": MEMORIA_VALIDATED_COMMIT,
        "sizes": list(SIZES),
        "contract": {
            "measurement_only": True,
            "no_new_abi": True,
            "bdr_format_unmodified": True,
            "frozen_memoria_write_shape": True,
            "performance_thresholds": False,
        },
        "samples": samples,
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
