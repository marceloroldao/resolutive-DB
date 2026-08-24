import os
import shutil
import tempfile
import unittest

from bdr_native import BDRException, Database


class TestPythonErrorContract(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp(prefix="bdr-v65-")

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def test_missing_key_returns_none(self):
        with Database(self.root, reserve_bytes=2 * 1024 * 1024,
                      wal_batch=32, partition_count=32) as db:
            self.assertIsNone(db.get(b"missing"))

    def test_empty_key_rejected(self):
        with Database(self.root, reserve_bytes=2 * 1024 * 1024,
                      wal_batch=32, partition_count=32) as db:
            with self.assertRaises(BDRException):
                db.put_sync(b"", b"value")

    def test_future_ticket_rejected(self):
        with Database(self.root, reserve_bytes=2 * 1024 * 1024,
                      wal_batch=32, partition_count=32) as db:
            db.put_sync(b"a", b"1")
            with self.assertRaises(BDRException):
                db.wait(db.last_sequence + 1000)

    def test_invalid_partition_count_rejected(self):
        with self.assertRaises(BDRException):
            Database(self.root, reserve_bytes=2 * 1024 * 1024,
                     wal_batch=32, partition_count=0)

    def test_invalid_partition_load_rejected(self):
        with self.assertRaises(BDRException):
            Database(self.root, reserve_bytes=2 * 1024 * 1024,
                     wal_batch=32, partition_count=32,
                     partition_max_load=0.99)

    def test_zero_wal_batch_rejected(self):
        with self.assertRaises(BDRException):
            Database(self.root, reserve_bytes=2 * 1024 * 1024,
                     wal_batch=0, partition_count=32)

    def test_use_after_close_rejected(self):
        db = Database(self.root, reserve_bytes=2 * 1024 * 1024,
                      wal_batch=32, partition_count=32)
        db.put_sync(b"a", b"1")
        db.close()
        for operation in (
            lambda: db.get(b"a"),
            lambda: db.put(b"b", b"2"),
            lambda: db.sync(),
            lambda: db.checkpoint(),
        ):
            with self.assertRaises(BDRException):
                operation()
        # Close is intentionally idempotent.
        db.close()

    def test_update_does_not_increase_cardinality(self):
        with Database(self.root, reserve_bytes=2 * 1024 * 1024,
                      wal_batch=32, partition_count=8) as db:
            db.put_sync(b"same", b"v1")
            self.assertEqual(db.size, 1)
            db.put_sync(b"same", b"v2")
            self.assertEqual(db.size, 1)
            self.assertEqual(db.get(b"same"), b"v2")
            db.checkpoint()

        with Database(self.root, reserve_bytes=2 * 1024 * 1024,
                      wal_batch=32, partition_count=8) as db:
            self.assertEqual(db.size, 1)
            self.assertEqual(db.get(b"same"), b"v2")


if __name__ == "__main__":
    unittest.main(verbosity=2)
