import json
import os
from pathlib import Path

import pytest

from bdr.atomic import AtomicBDR, AtomicDiagnostics, BatchResult, DurabilityMode, Operation


pytestmark = pytest.mark.skipif(
    not os.environ.get("BDR_ATOMIC_LIBRARY"),
    reason="BDR_ATOMIC_LIBRARY is required for native bridge integration tests",
)


def test_binary_utf8_batch_sequence_and_reopen(tmp_path: Path):
    db_path = tmp_path / "atomic-bdr"
    db = AtomicBDR.open(db_path)
    first = db.put_many(
        {
            "nó:azul": b"\x00\x01\xffpayload\x00",
            "texto:olá": "ação temporal",
            b"empty-value": b"",
        }
    )
    assert isinstance(first, BatchResult)
    assert first.operations == 3
    assert first.durable is True
    assert first.sequence > 0
    assert db.last_sequence() == first.sequence
    assert db.durable_sequence() == first.sequence
    assert db.get("nó:azul") == b"\x00\x01\xffpayload\x00"
    assert db.get("texto:olá") == "ação temporal".encode("utf-8")
    assert db.get(b"empty-value") == b""

    second = db.write_batch(
        [Operation.delete("texto:olá"), Operation.put("estado:camisa", "preta")],
        durability=DurabilityMode.BATCH_SYNC,
    )
    assert second.sequence > first.sequence
    assert second.operations == 2
    assert db.get("texto:olá") is None
    assert db.get("estado:camisa") == b"preta"
    last_before_close = db.last_sequence()
    durable_before_close = db.durable_sequence()
    db.close()

    reopened = AtomicBDR.open(db_path)
    try:
        assert reopened.get("nó:azul") == b"\x00\x01\xffpayload\x00"
        assert reopened.get("texto:olá") is None
        assert reopened.get("estado:camisa") == b"preta"
        assert reopened.last_sequence() == last_before_close
        assert reopened.durable_sequence() == durable_before_close
        diagnostics = reopened.diagnostics()
        assert isinstance(diagnostics, AtomicDiagnostics)
        assert diagnostics.replayed_batches == 2
        assert diagnostics.replayed_operations == 5
        assert diagnostics.resident_records == 3
        assert diagnostics.wal_bytes > 0
        assert diagnostics.last_sequence == last_before_close
        assert diagnostics.durable_sequence == durable_before_close
        assert diagnostics.wal_replay_us >= diagnostics.wal_read_us
        assert diagnostics.wal_replay_us >= diagnostics.wal_decode_apply_us
    finally:
        reopened.close()


def test_async_then_sync_advances_durable_boundary(tmp_path: Path):
    db = AtomicBDR.open(tmp_path / "async-bdr")
    try:
        result = db.write_batch(
            [Operation.put("event:1", b"async")],
            durability=DurabilityMode.ASYNC,
        )
        assert result.durable is False
        assert db.last_sequence() == result.sequence
        assert db.durable_sequence() <= db.last_sequence()
        synced = db.sync()
        assert synced == db.last_sequence()
        assert db.durable_sequence() == db.last_sequence()
    finally:
        db.close()


def test_memoria_topological_temporal_acceptance_fixture(tmp_path: Path):
    db_path = tmp_path / "memoria-topology"
    events = [
        (12, "azul", "Minha camisa é azul."),
        (24, "preta", "Minha camisa é preta."),
        (31, "branca", "Minha camisa é branca."),
    ]

    db = AtomicBDR.open(db_path)
    try:
        address = b"addr:entity:minha_camisa"
        previous_sequence = db.last_sequence()
        for logical_seq, color, raw in events:
            payload = json.dumps(
                {
                    "sequence_time": logical_seq,
                    "entity_address": address.decode(),
                    "attribute": "cor",
                    "value": color,
                },
                ensure_ascii=False,
                sort_keys=True,
            ).encode("utf-8")
            result = db.write_batch(
                [
                    Operation.put(f"event:{logical_seq}", payload),
                    Operation.put(f"raw:{logical_seq}", raw),
                    Operation.put(f"state:minha_camisa:cor:{logical_seq}", color),
                    Operation.put("address:minha_camisa", address),
                    Operation.put("metadata:next_sequence_time", str(logical_seq + 1)),
                ]
            )
            assert result.operations == 5
            assert result.sequence > previous_sequence
            previous_sequence = result.sequence
        native_last = db.last_sequence()
    finally:
        db.close()

    reopened = AtomicBDR.open(db_path)
    try:
        recovered = []
        for logical_seq, expected_color, expected_raw in events:
            event = json.loads(reopened.get(f"event:{logical_seq}").decode("utf-8"))
            raw = reopened.get(f"raw:{logical_seq}").decode("utf-8")
            assert event["value"] == expected_color
            assert raw == expected_raw
            assert reopened.get("address:minha_camisa") == b"addr:entity:minha_camisa"
            recovered.append((event["sequence_time"], event["value"]))

        recovered.sort()
        history = [color for _, color in recovered]
        assert history == ["azul", "preta", "branca"]
        assert history[-1] == "branca"
        assert history[-2] == "preta"
        assert int(reopened.get("metadata:next_sequence_time")) > 31
        assert reopened.last_sequence() == native_last
        assert reopened.durable_sequence() == native_last
    finally:
        reopened.close()
