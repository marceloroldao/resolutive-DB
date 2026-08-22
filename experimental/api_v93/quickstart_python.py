from pathlib import Path
import shutil
from bdr_native import Database

path = Path("./v93_py_db")
shutil.rmtree(path, ignore_errors=True)

db = Database(path)
t = db.put(b"hello", b"world")
db.wait(t)
assert db.get(b"hello") == b"world"
db.checkpoint()
db.close()

db = Database(path)
assert db.get(b"hello") == b"world"
db.delete_sync(b"hello")
assert db.get(b"hello") is None
db.close()

print("V93 Python quickstart PASS")
