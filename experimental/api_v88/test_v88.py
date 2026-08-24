from pathlib import Path
import shutil
import tempfile

from bdr_native import Database, BdrError, BDR_C_ABI_VERSION

root = Path(tempfile.mkdtemp(prefix="bdr-v88-"))
try:
    dbdir = root / "db"
    with Database(dbdir) as db:
        assert BDR_C_ABI_VERSION == 1
        t = db.put(b"k\x00async", b"v\x00async")
        assert t > 0
        db.wait(t)
        assert db.get(b"k\x00async") == b"v\x00async"

        db.put_sync(b"empty", b"")
        assert db.get(b"empty") == b""

        db.put_sync("utf8-key", "valor")
        assert db.get("utf8-key") == b"valor"

        dt = db.delete(b"k\x00async")
        db.wait(dt)
        assert db.get(b"k\x00async") is None

        db.checkpoint()
        assert db.last_sequence == db.durable_sequence
        seq = db.last_sequence
        size = len(db)

    with Database(dbdir) as db:
        assert db.get(b"empty") == b""
        assert db.get("utf8-key") == b"valor"
        assert db.get(b"k\x00async") is None
        assert db.last_sequence == seq
        assert db.durable_sequence == seq
        assert len(db) == size
        db.put_sync(b"after-reopen", b"ok")
        db.checkpoint()

    db = Database(dbdir)
    db.close()
    try:
        db.get(b"x")
        raise AssertionError("use-after-close should fail")
    except BdrError:
        pass

    print("V88 PASS: installed wheel + ABI v1 + binary data + reopen")
finally:
    shutil.rmtree(root, ignore_errors=True)
