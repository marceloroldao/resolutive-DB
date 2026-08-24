import os
import shutil
import tempfile
import unittest

from bdr_native import Database


class TestBDRNative(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp(prefix="bdr-v62-")

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def test_binary_roundtrip_checkpoint_reopen(self):
        key = b"python\x00binary\xffkey"
        value = b"value\x00with\x00zeros\xfe"
        empty_value_key = b"empty-value"

        with Database(self.root, reserve_bytes=8 * 1024 * 1024,
                      wal_batch=64, partition_count=64,
                      partition_max_load=0.78) as db:
            ticket = db.put(key, value)
            self.assertGreater(ticket, 0)
            self.assertEqual(db.get(key), value)  # visibility precedes durability
            db.wait(ticket)
            self.assertGreaterEqual(db.durable_sequence, ticket)

            db.put_sync(empty_value_key, b"")
            self.assertEqual(db.get(empty_value_key), b"")

            db.put_sync("utf8-key", "valor")
            self.assertEqual(db.get("utf8-key"), b"valor")
            db.delete_sync(key)
            self.assertIsNone(db.get(key))

            db.checkpoint()
            self.assertEqual(db.size, 2)
            self.assertEqual(db.last_sequence, db.durable_sequence)
            seq = db.last_sequence

        with Database(self.root, reserve_bytes=8 * 1024 * 1024,
                      wal_batch=64, partition_count=64) as db:
            self.assertIsNone(db.get(key))
            self.assertEqual(db.get(empty_value_key), b"")
            self.assertEqual(db.get("utf8-key"), b"valor")
            self.assertEqual(db.size, 2)
            self.assertEqual(db.last_sequence, seq)
            self.assertEqual(db.durable_sequence, seq)

            t = db.put(b"after-reopen", b"ok")
            db.wait(t)
            self.assertEqual(db.get(b"after-reopen"), b"ok")


if __name__ == "__main__":
    unittest.main(verbosity=2)
