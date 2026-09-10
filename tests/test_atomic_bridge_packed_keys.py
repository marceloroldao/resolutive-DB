import os
from pathlib import Path

import pytest

from bdr.atomic import AtomicBDR


pytestmark = pytest.mark.skipif(
    not os.environ.get("BDR_ATOMIC_LIBRARY"),
    reason="BDR_ATOMIC_LIBRARY is required for native bridge integration tests",
)


def test_packed_bulk_keys_preserve_binary_utf8_order_and_missing(tmp_path: Path):
    db = AtomicBDR.open(tmp_path / "packed-bulk-keys")
    try:
        db.put_many(
            [
                (b"bin\x00key", b"binary-value"),
                ("nó:ação", "valor-utf8"),
                (b"plain", b""),
            ]
        )
        assert db.get_many(
            [b"bin\x00key", b"missing\x00key", "nó:ação", b"plain", b"bin\x00key"]
        ) == [
            b"binary-value",
            None,
            "valor-utf8".encode("utf-8"),
            b"",
            b"binary-value",
        ]
    finally:
        db.close()
