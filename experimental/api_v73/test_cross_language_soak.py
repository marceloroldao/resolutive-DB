import ctypes as C
import os
import shutil
import subprocess
import tempfile

from bdr_native import Database

ROUNDS = 12
PER_ROUND = 200


def build_c_writer(source_path: str) -> None:
    code = r'''
#include <bdr/c_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc != 4) return 2;
    const char* path = argv[1];
    int round = atoi(argv[2]);
    int n = atoi(argv[3]);
    bdr_options o = bdr_default_options();
    o.reserve_bytes = 8u * 1024u * 1024u;
    o.wal_batch = 64;
    o.partition_count = 128;
    o.partition_max_load = 0.78;
    bdr_db* db = NULL;
    if (bdr_open(path, &o, &db) != BDR_OK) return 3;
    for (int i=0;i<n;++i) {
        char key[96], value[96];
        snprintf(key,sizeof(key),"c-%02d-%04d",round,i);
        snprintf(value,sizeof(value),"cv-%02d-%04d",round,i);
        if (bdr_put_sync(db,key,strlen(key),value,strlen(value)) != BDR_OK) return 4;
    }
    if (bdr_checkpoint(db) != BDR_OK) return 5;
    if (bdr_close(db) != BDR_OK) return 6;
    return 0;
}
'''
    with open(source_path, 'w', encoding='utf-8') as f:
        f.write(code)


def main() -> int:
    root = tempfile.mkdtemp(prefix='bdr-v73-')
    oracle: dict[bytes, bytes] = {}
    try:
        c_src = os.path.join(root, 'c_writer.c')
        build_c_writer(c_src)
        c_bin = os.path.join(root, 'c_writer')
        include_dir = os.environ['BDR_INCLUDE_DIR']
        lib_dir = os.environ['BDR_LIB_DIR']
        subprocess.check_call([
            'g++', '-std=c++20', c_src, '-I' + include_dir, '-L' + lib_dir,
            '-Wl,-rpath,' + lib_dir, '-lbdr_c', '-o', c_bin
        ])

        db_path = os.path.join(root, 'db')
        for r in range(ROUNDS):
            # Python phase
            with Database(db_path, reserve_bytes=8*1024*1024, wal_batch=64,
                          partition_count=128, partition_max_load=0.78) as db:
                for i in range(PER_ROUND):
                    k = f'py-{r:02d}-{i:04d}'.encode()
                    v = f'pv-{r:02d}-{i:04d}'.encode()
                    db.put_sync(k, v)
                    oracle[k] = v
                if r % 3 == 2:
                    victims = sorted(oracle)[:min(50, len(oracle))]
                    for k in victims:
                        db.delete_sync(k)
                        oracle.pop(k, None)
                db.checkpoint()

            # C phase on same on-disk database.
            subprocess.check_call([c_bin, db_path, str(r), str(PER_ROUND)])
            for i in range(PER_ROUND):
                k = f'c-{r:02d}-{i:04d}'.encode()
                oracle[k] = f'cv-{r:02d}-{i:04d}'.encode()

            # Python reopens state written by both APIs and validates all known data.
            with Database(db_path, reserve_bytes=8*1024*1024, wal_batch=64,
                          partition_count=128, partition_max_load=0.78) as db:
                if db.size != len(oracle):
                    raise AssertionError((r, 'size', db.size, len(oracle)))
                for k, v in oracle.items():
                    got = db.get(k)
                    if got != v:
                        raise AssertionError((r, k, got, v))
                if db.last_sequence != db.durable_sequence:
                    raise AssertionError((r, 'frontier', db.last_sequence, db.durable_sequence))

        print('rounds,records,c_python_cross_reopen,sequence_frontier,pass')
        print(f'{ROUNDS},{len(oracle)},1,1,1')
        return 0
    finally:
        shutil.rmtree(root, ignore_errors=True)


if __name__ == '__main__':
    raise SystemExit(main())
