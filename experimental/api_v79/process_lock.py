from pathlib import Path
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: process_lock.py <input.cpp> <output.cpp>")

src = Path(sys.argv[1]).read_text()

repls = []
repls.append((
    '#include <stdexcept>\n#include <sys/stat.h>',
    '#include <stdexcept>\n#include <sys/file.h>\n#include <sys/stat.h>'
))
repls.append((
    'int wal_fd=-1;off_t wal_off=0;fs::path active_wal;uint64_t segment_id=0;',
    'int wal_fd=-1,lock_fd=-1;off_t wal_off=0;fs::path active_wal;uint64_t segment_id=0;'
))

old_ctor = 'Impl(fs::path d,Options o):dir(std::move(d)),options(o),index(o.partition_count,o.partition_max_load){if(!options.wal_batch)throw std::invalid_argument("wal_batch must be > 0");fs::create_directories(dir);recover();writer=std::thread([this]{writer_loop();});}'
new_ctor = '''Impl(fs::path d,Options o):dir(std::move(d)),options(o),index(o.partition_count,o.partition_max_load){
        if(!options.wal_batch)throw std::invalid_argument("wal_batch must be > 0");
        fs::create_directories(dir);
        const fs::path lock_path=dir/"LOCK";
        lock_fd=::open(lock_path.c_str(),O_CREAT|O_RDWR,0644);
        if(lock_fd<0)throw std::runtime_error("BDR lock file open failed");
        if(::flock(lock_fd,LOCK_EX|LOCK_NB)!=0){
            ::close(lock_fd);lock_fd=-1;
            throw std::runtime_error("BDR database is already open by another process");
        }
        try{
            recover();
            writer=std::thread([this]{writer_loop();});
        }catch(...){
            ::flock(lock_fd,LOCK_UN);::close(lock_fd);lock_fd=-1;
            throw;
        }
    }'''
repls.append((old_ctor,new_ctor))

# This patch is intended to run after V74 hardening, whose close_impl has a
# deterministic final rethrow block. Release the process lock before rethrowing.
old_tail = '''        if(close_error)std::rethrow_exception(close_error);
    }
'''
new_tail = '''        if(lock_fd>=0){::flock(lock_fd,LOCK_UN);::close(lock_fd);lock_fd=-1;}
        if(close_error)std::rethrow_exception(close_error);
    }
'''
repls.append((old_tail,new_tail))

for old,new in repls:
    count=src.count(old)
    if count!=1:
        raise SystemExit(f"expected one match, got {count}: {old[:100]!r}")
    src=src.replace(old,new)

out=Path(sys.argv[2]);out.parent.mkdir(parents=True,exist_ok=True);out.write_text(src)
print(f"process-lock patch: {sys.argv[1]} -> {sys.argv[2]}")
