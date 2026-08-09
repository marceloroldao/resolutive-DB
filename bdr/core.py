"""Core implementation for the Resolutive Database (BDR) proof of concept.

The PoC implements deterministic direct addressing from a canonical key into a
fixed number of harmonic buckets. Each bucket stores a secondary mapping keyed
by a phase signature. Average lookup is O(1) under the same assumptions as a
hash table: bounded-cost encoding and non-adversarial hash distribution.

This module deliberately does not claim worst-case O(1) for arbitrary loads.
That claim must be established empirically and mathematically for any stronger
future implementation.
"""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import math
import threading
from typing import Dict, Iterable, Iterator, Optional, Tuple, Union

KeyLike = Union[str, bytes, bytearray, memoryview, int, float]
PayloadLike = Union[bytes, bytearray, memoryview, str]


def _canonical_bytes(value: KeyLike) -> bytes:
    """Return a stable byte representation suitable for deterministic encoding."""
    if isinstance(value, bytes):
        return value
    if isinstance(value, (bytearray, memoryview)):
        return bytes(value)
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, int):
        return f"i:{value}".encode("ascii")
    if isinstance(value, float):
        if not math.isfinite(value):
            raise ValueError("float keys must be finite")
        return f"f:{value.hex()}".encode("ascii")
    raise TypeError(f"unsupported key type: {type(value)!r}")


def _payload_bytes(value: PayloadLike) -> bytes:
    if isinstance(value, bytes):
        return value
    if isinstance(value, (bytearray, memoryview)):
        return bytes(value)
    if isinstance(value, str):
        return value.encode("utf-8")
    raise TypeError(f"unsupported payload type: {type(value)!r}")


@dataclass(frozen=True, slots=True)
class EnderecoResolutivo:
    """Quantized resolutive address used by the PoC engine."""

    rho_R: int
    phi: int
    theta: float
    f_nu: float
    fingerprint: bytes


@dataclass(frozen=True, slots=True)
class EntidadeResolutiva:
    """Self-contained data block stored by the engine."""

    id: int
    rho_R: int
    phi: int
    theta: float
    f_nu: float
    payload: bytes
    fingerprint: bytes


class EncoderResolutivo:
    """Deterministically maps keys to (rho_R, phi, theta, f_nu).

    The mapping is a software engineering construction inspired by the proposed
    resolutive address space. It is not presented as an experimentally validated
    physical law.
    """

    def __init__(self, bucket_count: int = 1 << 16, phase_bits: int = 64) -> None:
        if bucket_count <= 0 or bucket_count & (bucket_count - 1):
            raise ValueError("bucket_count must be a positive power of two")
        if not 16 <= phase_bits <= 128:
            raise ValueError("phase_bits must be between 16 and 128")
        self.bucket_count = bucket_count
        self.phase_bits = phase_bits
        self._bucket_mask = bucket_count - 1
        self._phase_mask = (1 << phase_bits) - 1

    def encode(self, key: KeyLike) -> EnderecoResolutivo:
        raw = _canonical_bytes(key)
        digest = hashlib.blake2b(raw, digest_size=32, person=b"BDR-v1").digest()

        lane0 = int.from_bytes(digest[0:8], "big")
        lane1 = int.from_bytes(digest[8:16], "big")
        lane2 = int.from_bytes(digest[16:24], "big")
        lane3 = int.from_bytes(digest[24:32], "big")

        rho_R = lane0 & self._bucket_mask
        phi = lane1 & self._phase_mask

        # Normalized observables retained as metadata for future experiments.
        theta = (lane2 / ((1 << 64) - 1)) * (2.0 * math.pi)
        f_nu = lane3 / ((1 << 64) - 1)

        # 128-bit exact-key guard makes accidental phase collisions detectable.
        fingerprint = hashlib.blake2b(raw, digest_size=16, person=b"BDR-key-v1").digest()
        return EnderecoResolutivo(rho_R, phi, theta, f_nu, fingerprint)


class BancoDeDadosResolutivo:
    """Thread-safe in-memory BDR proof-of-concept engine.

    Storage layout:
        direct bucket array -> phase dictionary -> fingerprint dictionary -> entity

    Direct bucket selection is O(1). The nested Python dictionaries have average
    O(1) lookup semantics; therefore the overall PoC has expected/average O(1)
    lookup, not an unconditional worst-case O(1) guarantee.
    """

    def __init__(self, bucket_count: int = 1 << 16, phase_bits: int = 64) -> None:
        self.encoder = EncoderResolutivo(bucket_count=bucket_count, phase_bits=phase_bits)
        self._buckets: list[Optional[Dict[int, Dict[bytes, EntidadeResolutiva]]]] = [
            None
        ] * bucket_count
        self._size = 0
        self._next_id = 1
        self._lock = threading.RLock()

    def __len__(self) -> int:
        return self._size

    def _locate(self, key: KeyLike) -> Tuple[EnderecoResolutivo, Optional[Dict[bytes, EntidadeResolutiva]]]:
        address = self.encoder.encode(key)
        bucket = self._buckets[address.rho_R]
        if bucket is None:
            return address, None
        return address, bucket.get(address.phi)

    def inserir(self, key: KeyLike, payload: PayloadLike) -> EntidadeResolutiva:
        """Insert or replace an entity and return the stored immutable block."""
        address = self.encoder.encode(key)
        data = _payload_bytes(payload)
        with self._lock:
            bucket = self._buckets[address.rho_R]
            if bucket is None:
                bucket = {}
                self._buckets[address.rho_R] = bucket
            phase_slot = bucket.setdefault(address.phi, {})
            existing = phase_slot.get(address.fingerprint)
            entity_id = existing.id if existing else self._next_id
            if existing is None:
                self._next_id += 1
                self._size += 1
            entity = EntidadeResolutiva(
                id=entity_id,
                rho_R=address.rho_R,
                phi=address.phi,
                theta=address.theta,
                f_nu=address.f_nu,
                payload=data,
                fingerprint=address.fingerprint,
            )
            phase_slot[address.fingerprint] = entity
            return entity

    def buscar(self, key: KeyLike) -> Optional[EntidadeResolutiva]:
        """Return the exact matching entity, or None if absent."""
        address, phase_slot = self._locate(key)
        if phase_slot is None:
            return None
        return phase_slot.get(address.fingerprint)

    def obter(self, key: KeyLike) -> EntidadeResolutiva:
        entity = self.buscar(key)
        if entity is None:
            raise KeyError(key)
        return entity

    def remover(self, key: KeyLike) -> bool:
        """Remove a key. Returns True only when an entity existed."""
        address = self.encoder.encode(key)
        with self._lock:
            bucket = self._buckets[address.rho_R]
            if bucket is None:
                return False
            phase_slot = bucket.get(address.phi)
            if phase_slot is None or address.fingerprint not in phase_slot:
                return False
            del phase_slot[address.fingerprint]
            self._size -= 1
            if not phase_slot:
                del bucket[address.phi]
            if not bucket:
                self._buckets[address.rho_R] = None
            return True

    def contem(self, key: KeyLike) -> bool:
        return self.buscar(key) is not None

    def limpar(self) -> None:
        """Release all occupied bucket structures, leaving no residual entities."""
        with self._lock:
            self._buckets = [None] * self.encoder.bucket_count
            self._size = 0
            self._next_id = 1

    def estatisticas(self) -> dict[str, int | float]:
        occupied = 0
        phase_slots = 0
        max_phase_collisions = 0
        for bucket in self._buckets:
            if bucket:
                occupied += 1
                phase_slots += len(bucket)
                for slot in bucket.values():
                    max_phase_collisions = max(max_phase_collisions, len(slot))
        return {
            "records": self._size,
            "bucket_count": self.encoder.bucket_count,
            "occupied_buckets": occupied,
            "load_factor": self._size / self.encoder.bucket_count,
            "phase_slots": phase_slots,
            "max_exact_collisions": max_phase_collisions,
        }

    def entidades(self) -> Iterator[EntidadeResolutiva]:
        for bucket in self._buckets:
            if not bucket:
                continue
            for phase_slot in bucket.values():
                yield from phase_slot.values()

    def inserir_lote(self, items: Iterable[Tuple[KeyLike, PayloadLike]]) -> int:
        count = 0
        for key, payload in items:
            self.inserir(key, payload)
            count += 1
        return count
