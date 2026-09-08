from __future__ import annotations

import ctypes
import os
import tempfile
from pathlib import Path


BDR_ATOMIC_C_OK = 0
BDR_ATOMIC_C_PUT = 1


class Operation(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_int),
        ("key", ctypes.c_void_p),
        ("key_size", ctypes.c_size_t),
        ("value", ctypes.c_void_p),
        ("value_size", ctypes.c_size_t),
    ]


class Buffer(ctypes.Structure):
    _fields_ = [
        ("data", ctypes.POINTER(ctypes.c_uint8)),
        ("size", ctypes.c_size_t),
    ]


class BatchResult(ctypes.Structure):
    _fields_ = [
        ("sequence", ctypes.c_uint64),
        ("operations", ctypes.c_size_t),
        ("durable", ctypes.c_int),
    ]


def _load(path: str) -> ctypes.CDLL:
    lib = ctypes.CDLL(path)
    lib.bdr_atomic_c_abi_version.restype = ctypes.c_uint32
    lib.bdr_atomic_c_open.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
    lib.bdr_atomic_c_open.restype = ctypes.c_int
    lib.bdr_atomic_c_write_batch.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(Operation),
        ctypes.c_size_t,
        ctypes.POINTER(BatchResult),
    ]
    lib.bdr_atomic_c_write_batch.restype = ctypes.c_int
    lib.bdr_atomic_c_get.argtypes = [
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.POINTER(Buffer),
    ]
    lib.bdr_atomic_c_get.restype = ctypes.c_int
    lib.bdr_atomic_c_last_sequence.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint64)]
    lib.bdr_atomic_c_last_sequence.restype = ctypes.c_int
    lib.bdr_atomic_c_durable_sequence.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint64)]
    lib.bdr_atomic_c_durable_sequence.restype = ctypes.c_int
    lib.bdr_atomic_c_free_buffer.argtypes = [Buffer]
    lib.bdr_atomic_c_close.argtypes = [ctypes.c_void_p]
    return lib


def _check(status: int, operation: str) -> None:
    if status != BDR_ATOMIC_C_OK:
        raise RuntimeError(f"{operation} failed with BDR status {status}")


def _open(lib: ctypes.CDLL, directory: Path) -> ctypes.c_void_p:
    handle = ctypes.c_void_p()
    _check(lib.bdr_atomic_c_open(os.fsencode(directory), ctypes.byref(handle)), "open")
    if not handle.value:
        raise RuntimeError("BDR returned a null handle")
    return handle


def _get(lib: ctypes.CDLL, handle: ctypes.c_void_p, key: bytes) -> bytes:
    key_buffer = ctypes.create_string_buffer(key)
    out = Buffer()
    _check(
        lib.bdr_atomic_c_get(
            handle,
            ctypes.cast(key_buffer, ctypes.c_void_p),
            len(key),
            ctypes.byref(out),
        ),
        "get",
    )
    try:
        return ctypes.string_at(out.data, out.size) if out.size else b""
    finally:
        lib.bdr_atomic_c_free_buffer(out)


def main() -> None:
    library_path = os.environ.get("BDR_ATOMIC_LIB")
    if not library_path:
        raise RuntimeError("BDR_ATOMIC_LIB must point to the shared atomic C ABI library")

    lib = _load(library_path)
    if lib.bdr_atomic_c_abi_version() != 1:
        raise RuntimeError("unexpected atomic C ABI version")

    key_a = "memoria/nó/çã".encode("utf-8")
    value_a = b"\x00\xfftopology\x00payload"
    key_b = b"memoria/event/31"
    value_b = "branca — usuário confirmado".encode("utf-8")

    with tempfile.TemporaryDirectory(prefix="bdr-ctypes-") as temp_dir:
        root = Path(temp_dir) / "db"
        handle = _open(lib, root)
        try:
            keepalive: list[ctypes.Array] = []
            operations = (Operation * 2)()
            for index, (key, value) in enumerate(((key_a, value_a), (key_b, value_b))):
                key_buffer = ctypes.create_string_buffer(key)
                value_buffer = ctypes.create_string_buffer(value)
                keepalive.extend((key_buffer, value_buffer))
                operations[index] = Operation(
                    BDR_ATOMIC_C_PUT,
                    ctypes.cast(key_buffer, ctypes.c_void_p),
                    len(key),
                    ctypes.cast(value_buffer, ctypes.c_void_p),
                    len(value),
                )

            result = BatchResult()
            _check(lib.bdr_atomic_c_write_batch(handle, operations, 2, ctypes.byref(result)), "write_batch")
            if result.sequence < 1 or result.operations != 2 or result.durable != 1:
                raise AssertionError(f"unexpected batch result: {result.sequence=}, {result.operations=}, {result.durable=}")
            written_sequence = int(result.sequence)
            if _get(lib, handle, key_a) != value_a:
                raise AssertionError("binary round-trip failed before reopen")
        finally:
            lib.bdr_atomic_c_close(handle)

        reopened = _open(lib, root)
        try:
            if _get(lib, reopened, key_a) != value_a:
                raise AssertionError("binary round-trip failed after reopen")
            if _get(lib, reopened, key_b) != value_b:
                raise AssertionError("UTF-8 round-trip failed after reopen")

            last = ctypes.c_uint64()
            durable = ctypes.c_uint64()
            _check(lib.bdr_atomic_c_last_sequence(reopened, ctypes.byref(last)), "last_sequence")
            _check(lib.bdr_atomic_c_durable_sequence(reopened, ctypes.byref(durable)), "durable_sequence")
            if int(last.value) != written_sequence or int(durable.value) != written_sequence:
                raise AssertionError(
                    f"sequence mismatch after reopen: expected={written_sequence}, last={last.value}, durable={durable.value}"
                )
        finally:
            lib.bdr_atomic_c_close(reopened)

    print("atomic shared ctypes smoke: PASS")


if __name__ == "__main__":
    main()
