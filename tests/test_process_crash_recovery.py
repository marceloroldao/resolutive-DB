import os
import signal
import subprocess
import sys
import time
from pathlib import Path

import pytest

from bdr.persistent_engine import PersistentBDR


@pytest.mark.skipif(sys.platform == "win32", reason="SIGKILL crash test requires POSIX")
def test_sigkill_preserves_durable_prefix(tmp_path: Path):
    root = tmp_path / "db"
    marker = tmp_path / "durable.marker"

    child = r'''
import os, sys, time
from pathlib import Path
from bdr.persistent_engine import PersistentBDR
root = Path(sys.argv[1])
marker = Path(sys.argv[2])
marker_tmp = marker.with_suffix(".tmp")
db = PersistentBDR(root, segment_ops=64, group_commit_ops=16)
for i in range(1000000):
    # Every 8th write is explicitly durable. The marker is published atomically
    # only after the database fsync completes, so it is a lower bound on the
    # state that must survive SIGKILL.
    durable = (i % 8 == 0)
    db.put(f"k{i}", i, durable=durable)
    if durable:
        with open(marker_tmp, "w") as fh:
            fh.write(str(i))
            fh.flush()
            os.fsync(fh.fileno())
        os.replace(marker_tmp, marker)
        dir_fd = os.open(marker.parent, getattr(os, "O_DIRECTORY", 0))
        try:
            os.fsync(dir_fd)
        finally:
            os.close(dir_fd)
    time.sleep(0.00005)
'''

    proc = subprocess.Popen(
        [sys.executable, "-c", child, str(root), str(marker)],
        cwd=Path(__file__).resolve().parents[1],
    )

    deadline = time.time() + 10
    observed = None
    while time.time() < deadline:
        if marker.exists() and marker.stat().st_size:
            try:
                observed = int(marker.read_text())
                if observed >= 80:
                    break
            except ValueError:
                pass
        time.sleep(0.01)
    else:
        proc.kill()
        proc.wait(timeout=5)
        pytest.fail("child did not reach a durable prefix")

    os.kill(proc.pid, signal.SIGKILL)
    proc.wait(timeout=5)
    assert proc.returncode < 0

    durable_through = int(marker.read_text())
    assert durable_through >= (observed or 0)

    reopened = PersistentBDR(root, segment_ops=64, group_commit_ops=16)
    try:
        # Every write through the externally fsynced marker must survive.
        for i in range(durable_through + 1):
            assert reopened.get(f"k{i}") == i
    finally:
        reopened.close()
