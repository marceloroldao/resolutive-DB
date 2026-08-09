from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor

import pytest

from bdr import BancoDeDadosResolutivo, EncoderResolutivo


def test_encoder_is_deterministic() -> None:
    encoder = EncoderResolutivo(bucket_count=1024)
    assert encoder.encode("alpha") == encoder.encode("alpha")
    assert encoder.encode("alpha") != encoder.encode("beta")


def test_insert_lookup_and_replace() -> None:
    db = BancoDeDadosResolutivo(bucket_count=1024)
    first = db.inserir("k", b"v1")
    assert db.obter("k").payload == b"v1"
    second = db.inserir("k", b"v2")
    assert len(db) == 1
    assert second.id == first.id
    assert db.obter("k").payload == b"v2"


def test_missing_key() -> None:
    db = BancoDeDadosResolutivo(bucket_count=256)
    assert db.buscar("missing") is None
    with pytest.raises(KeyError):
        db.obter("missing")


def test_remove_cleans_empty_slots() -> None:
    db = BancoDeDadosResolutivo(bucket_count=256)
    db.inserir("a", "payload")
    assert db.remover("a") is True
    assert db.remover("a") is False
    assert len(db) == 0
    stats = db.estatisticas()
    assert stats["occupied_buckets"] == 0
    assert stats["phase_slots"] == 0


def test_clear_leaves_no_residual_entities() -> None:
    db = BancoDeDadosResolutivo(bucket_count=256)
    for i in range(100):
        db.inserir(i, f"value-{i}")
    db.limpar()
    assert len(db) == 0
    assert list(db.entidades()) == []
    assert db.estatisticas()["occupied_buckets"] == 0


def test_exact_reconstruction_fidelity() -> None:
    db = BancoDeDadosResolutivo(bucket_count=2048)
    payloads = {f"key-{i}": bytes((i + j) % 256 for j in range(64)) for i in range(2000)}
    for key, payload in payloads.items():
        db.inserir(key, payload)
    for key, expected in payloads.items():
        assert db.obter(key).payload == expected


def test_high_density_small_bucket_array_remains_correct() -> None:
    db = BancoDeDadosResolutivo(bucket_count=8, phase_bits=16)
    for i in range(5000):
        db.inserir(f"dense-{i}", str(i))
    assert len(db) == 5000
    for i in range(0, 5000, 97):
        assert db.obter(f"dense-{i}").payload == str(i).encode()


def test_concurrent_insert_and_lookup() -> None:
    db = BancoDeDadosResolutivo(bucket_count=4096)

    def write(i: int) -> None:
        db.inserir(f"key-{i}", f"value-{i}")

    with ThreadPoolExecutor(max_workers=8) as executor:
        list(executor.map(write, range(5000)))

    assert len(db) == 5000

    def read(i: int) -> bytes:
        return db.obter(f"key-{i}").payload

    with ThreadPoolExecutor(max_workers=8) as executor:
        results = list(executor.map(read, range(5000)))

    assert results[1234] == b"value-1234"
    assert results[-1] == b"value-4999"


def test_invalid_encoder_configuration() -> None:
    with pytest.raises(ValueError):
        EncoderResolutivo(bucket_count=1000)
    with pytest.raises(ValueError):
        EncoderResolutivo(bucket_count=1024, phase_bits=8)
