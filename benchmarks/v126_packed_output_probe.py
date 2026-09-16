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


def packed_keys(keys: list[str]) -> tuple[object, object]:
    raw_keys = [_to_bytes(key, field="key") for key in keys]
    if any(not raw for raw in raw_keys):
        raise ValueError("empty key")
    packed = b"".join(raw_keys)
    arena = ctypes.create_string_buffer(packed, len(packed))
    base = ctypes.addressof(arena)
    c_keys = (_CKey * len(raw_keys))()
    offset = 0
    for index, raw in enumerate(raw_keys):
        c_keys[index].data = base + offset
        c_keys[index].size = len(raw)
        offset += len(raw)
    return arena, c_keys


def packed_output_get_many(db: AtomicBDR, keys: list[str]) -> tuple[list[bytes | None], dict[str, float | int]]:
    if not hasattr(db._lib, "bdr_atomic_c_get_many_packed"):
        raise RuntimeError("packed output symbol not available")
    fn = db._lib.bdr_atomic_c_get_many_packed
    fn.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(_CKey),
        ctypes.c_size_t,
        ctypes.POINTER(_CBuffer),
        ctypes.POINTER(ctypes.c_size_t),
        ctypes.POINTER(ctypes.c_size_t),
        ctypes.POINTER(ctypes.c_int),
    ]
    fn.restype = ctypes.c_int

    started = time.perf_counter_ns()
    key_arena, c_keys = packed_keys(keys)
    key_marshalling_ms = elapsed_ms(started)

    out_arena = _CBuffer()
    offsets = (ctypes.c_size_t * len(keys))()
    sizes = (ctypes.c_size_t * len(keys))()
    found = (ctypes.c_int * len(keys))()

    started = time.perf_counter_ns()
    _raise_status(
        fn(db._handle, c_keys, len(keys), ctypes.byref(out_arena), offsets, sizes, found),
        "get_many_packed",
    )
    native_call_ms = elapsed_ms(started)

    values: list[bytes | None] = []
    started = time.perf_counter_ns()
    base = ctypes.addressof(out_arena.data.contents) if out_arena.size else 0
    for index in range(len(keys)):
        if not found[index]:
            values.append(None)
        elif sizes[index] == 0:
            values.append(b"")
        else:
            values.append(ctypes.string_at(base + offsets[index], sizes[index]))
    python_materialize_ms = elapsed_ms(started)

    started = time.perf_counter_ns()
    db._lib.bdr_atomic_c_free_buffer(out_arena)
    python_free_ms = elapsed_ms(started)

    return values, {
        "key_marshalling_ms": key_marshalling_ms,
        "native_call_ms": native_call_ms,
        "python_materialize_ms": python_materialize_ms,
        "python_free_ms": python_free_ms,
        "measured_total_ms": key_marshalling_ms + native_call_ms + python_materialize_ms + python_free_ms,
        "arena_bytes": int(out_arena.size),
        "found": sum(int(x) for x in found),
    }


def run_size(count: int, library: Path, root: Path) -> dict[str, object]:
    addresses, store = build_fixture(count)
    capture = CaptureBackend()
    save_snapshot_to_backend(capture, addresses, store)
    operations = [Operation.put(key, value) for key, value in capture.puts]
    keys = [key for key, _ in capture.puts]
    expected = [value for _, value in capture.puts]

    bdr_root = root / f"bdr-{count}"
    with AtomicBDR.open(bdr_root, library_path=library) as db:
        db.write_batch(operations, durability=DurabilityMode.BATCH_SYNC)

    with AtomicBDR.open(bdr_root, library_path=library) as db:
        started = time.perf_counter_ns()
        public_values = db.get_many(keys)
        public_ms = elapsed_ms(started)
        packed_values, packed_timing = packed_output_get_many(db, keys)

    if public_values != expected:
        raise AssertionError(f"public bulk read parity failed at size {count}")
    if packed_values != expected:
        raise AssertionError(f"packed output parity failed at size {count}")

    return {
        "observations": count,
        "physical_records": len(keys),
        "serialized_value_bytes": sum(len(value) for value in expected),
        "public_get_many_ms": public_ms,
        "packed_output": packed_timing,
        "public_to_packed_ratio": public_ms / float(packed_timing["measured_total_ms"]),
        "parity": True,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True, type=Path)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="bdr-v126-") as temp_dir:
        root = Path(temp_dir)
        samples = [run_size(size, args.library, root) for size in SIZES]

    print(json.dumps({
        "benchmark": "v126_packed_output_probe",
        "memoria_validated_commit": MEMORIA_VALIDATED_COMMIT,
        "sizes": list(SIZES),
        "contract": {
            "experimental_symbol_only": True,
            "existing_get_many_unchanged": True,
            "bdr_format_unmodified": True,
            "frozen_memoria_write_shape": True,
            "performance_thresholds": False,
        },
        "samples": samples,
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
