from pathlib import Path
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: harden_writer.py <input.cpp> <output.cpp>")

src_path = Path(sys.argv[1])
out_path = Path(sys.argv[2])
s = src_path.read_text()

replacements = []

replacements.append((
    '#include <deque>\n#include <fcntl.h>',
    '#include <deque>\n#include <exception>\n#include <fcntl.h>'
))

replacements.append((
    'std::atomic<uint64_t>next_seq{0},durable_seq{0};bool stop=false,closed=false;std::thread writer;',
    'std::atomic<uint64_t>next_seq{0},durable_seq{0};bool stop=false,closed=false;std::exception_ptr writer_error;std::thread writer;'
))

old_submit = '''    Ticket submit_impl(Operation op){
        if(op.key.empty()||op.key.size()>(1u<<20)||op.value.size()>(1u<<24))throw std::invalid_argument("invalid BDR key/value size");
        std::lock_guard<std::mutex> order(submit_mu);
        if(closed)throw std::runtime_error("database closed");
        const uint64_t seq=next_seq.load(std::memory_order_acquire)+1;
        Pending pending{seq,op.type,std::move(op.key),std::move(op.value)};
        {
            std::unique_lock<std::mutex> qg(queue_mu);
            queue.push_back(std::move(pending));
            try{
                const Pending& accepted=queue.back();
                if(accepted.type==OperationType::Put)index.put(accepted.key,accepted.value);else index.erase(accepted.key);
                next_seq.store(seq,std::memory_order_release);
            }catch(...){
                queue.pop_back();
                throw;
            }
        }
        queue_cv.notify_one();
        return Ticket{seq};
    }
'''

new_submit = '''    Ticket submit_impl(Operation op){
        if(op.key.empty()||op.key.size()>(1u<<20)||op.value.size()>(1u<<24))throw std::invalid_argument("invalid BDR key/value size");
        std::lock_guard<std::mutex> order(submit_mu);
        if(closed)throw std::runtime_error("database closed");
        const uint64_t seq=next_seq.load(std::memory_order_acquire)+1;
        Pending pending{seq,op.type,std::move(op.key),std::move(op.value)};
        {
            std::unique_lock<std::mutex> qg(queue_mu);
            if(writer_error)std::rethrow_exception(writer_error);
            queue.push_back(std::move(pending));
            try{
                const Pending& accepted=queue.back();
                if(accepted.type==OperationType::Put)index.put(accepted.key,accepted.value);else index.erase(accepted.key);
                next_seq.store(seq,std::memory_order_release);
            }catch(...){
                queue.pop_back();
                throw;
            }
        }
        queue_cv.notify_one();
        return Ticket{seq};
    }
'''
replacements.append((old_submit, new_submit))

old_writer = '''    void writer_loop(){std::vector<Pending>local;std::vector<uint8_t>buf;buf.reserve(1<<20);for(;;){local.clear();{std::unique_lock<std::mutex>lk(queue_mu);queue_cv.wait(lk,[&]{return stop||!queue.empty();});if(stop&&queue.empty())break;while(!queue.empty()&&local.size()<options.wal_batch){local.push_back(std::move(queue.front()));queue.pop_front();}}buf.clear();for(const auto&p:local){auto r=make_record(p.seq,p.type,p.key,p.value);buf.insert(buf.end(),r.begin(),r.end());}{std::lock_guard<std::mutex>wg(wal_mu);pwrite_all(wal_fd,buf.data(),buf.size(),wal_off);wal_off+=off_t(buf.size());if(::fdatasync(wal_fd))std::abort();}durable_seq.store(local.back().seq);durable_cv.notify_all();}}
'''

new_writer = '''    void writer_loop(){
        std::vector<Pending>local;
        std::vector<uint8_t>buf;
        buf.reserve(1<<20);
        try{
            for(;;){
                local.clear();
                {
                    std::unique_lock<std::mutex>lk(queue_mu);
                    queue_cv.wait(lk,[&]{return stop||!queue.empty();});
                    if(stop&&queue.empty())break;
                    while(!queue.empty()&&local.size()<options.wal_batch){local.push_back(std::move(queue.front()));queue.pop_front();}
                }
                buf.clear();
                for(const auto&p:local){auto r=make_record(p.seq,p.type,p.key,p.value);buf.insert(buf.end(),r.begin(),r.end());}
                {
                    std::lock_guard<std::mutex>wg(wal_mu);
                    pwrite_all(wal_fd,buf.data(),buf.size(),wal_off);
                    wal_off+=off_t(buf.size());
                    if(::fdatasync(wal_fd))throw std::runtime_error("BDR fdatasync failed");
                }
                durable_seq.store(local.back().seq,std::memory_order_release);
                durable_cv.notify_all();
            }
        }catch(...){
            {
                std::lock_guard<std::mutex>lk(queue_mu);
                if(!writer_error)writer_error=std::current_exception();
                stop=true;
            }
            durable_cv.notify_all();
            queue_cv.notify_all();
        }
    }
'''
replacements.append((old_writer, new_writer))

old_wait = '    void wait_impl(Ticket t){if(!t)return;if(t.sequence>next_seq.load())throw std::invalid_argument("future ticket");std::unique_lock<std::mutex>lk(queue_mu);durable_cv.wait(lk,[&]{return durable_seq.load()>=t.sequence;});}\n'
new_wait = '''    void wait_impl(Ticket t){
        if(!t)return;
        if(t.sequence>next_seq.load())throw std::invalid_argument("future ticket");
        std::unique_lock<std::mutex>lk(queue_mu);
        durable_cv.wait(lk,[&]{return durable_seq.load(std::memory_order_acquire)>=t.sequence||writer_error;});
        if(durable_seq.load(std::memory_order_acquire)>=t.sequence)return;
        if(writer_error)std::rethrow_exception(writer_error);
        throw std::runtime_error("BDR durability wait interrupted");
    }
'''
replacements.append((old_wait, new_wait))

old_close = '''    void close_impl(){
        {
            std::lock_guard<std::mutex> sg(submit_mu);
            if(closed)return;
            closed=true;
        }
        sync_impl();
        {std::lock_guard<std::mutex>qg(queue_mu);stop=true;}
        queue_cv.notify_one();
        if(writer.joinable())writer.join();
        {std::lock_guard<std::mutex>wg(wal_mu);if(wal_fd>=0){::fdatasync(wal_fd);::close(wal_fd);wal_fd=-1;}}
    }
'''

new_close = '''    void close_impl(){
        {
            std::lock_guard<std::mutex>sg(submit_mu);
            if(closed)return;
            closed=true;
        }
        std::exception_ptr close_error;
        try{sync_impl();}catch(...){close_error=std::current_exception();}
        {
            std::lock_guard<std::mutex>qg(queue_mu);
            stop=true;
        }
        queue_cv.notify_all();
        durable_cv.notify_all();
        if(writer.joinable())writer.join();
        {
            std::lock_guard<std::mutex>wg(wal_mu);
            if(wal_fd>=0){
                if(!close_error&&::fdatasync(wal_fd))close_error=std::make_exception_ptr(std::runtime_error("BDR close fdatasync failed"));
                ::close(wal_fd);
                wal_fd=-1;
            }
        }
        if(close_error)std::rethrow_exception(close_error);
    }
'''
replacements.append((old_close, new_close))

for old, new in replacements:
    count = s.count(old)
    if count != 1:
        raise SystemExit(f"expected exactly one match, found {count}: {old[:80]!r}")
    s = s.replace(old, new)

out_path.parent.mkdir(parents=True, exist_ok=True)
out_path.write_text(s)
print(f"hardened {src_path} -> {out_path}")
