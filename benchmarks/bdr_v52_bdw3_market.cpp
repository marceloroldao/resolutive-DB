#define main v51_unused_main
#include "bdr_v51_bdw3_multiwriter_keep_size.cpp"
#undef main

#include <sqlite3.h>
#include <lmdb.h>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>

struct MR52{std::string engine;int writers;int window;double ops;uint64_t errors;};
static std::atomic<int> gnext52;
template<class F> static double timed52(F&&f){auto a=Clock::now();f();return std::chrono::duration<double>(Clock::now()-a).count();}

static MR52 run_bdr52(int total,int writers,int window){
    std::filesystem::remove("v52.bdw3");gnext52=0;
    double sec=timed52([&]{TicketWal3KeepSize wal("v52.bdw3",size_t(total)*96+4096,512);std::vector<std::thread>ts;for(int x=0;x<writers;++x)ts.emplace_back([&]{uint64_t last=0;int pending=0;for(;;){int i=gnext52.fetch_add(1);if(i>=total)break;last=wal.submit(K51(i),V51(i));if(++pending>=window){wal.wait(last);pending=0;}}if(pending)wal.wait(last);});for(auto&t:ts)t.join();});
    auto r=recover51("v52.bdw3",total);uint64_t e=r.bad+r.missing+r.duplicates+(r.good!=size_t(total))+(r.last!=uint64_t(total))+(!r.exact_eof);
    return{"BDR-v52-BDW3",writers,window,total/sec,e};
}

static void sqlite_retry52(sqlite3*d,const char*s){for(;;){int rc=sqlite3_exec(d,s,nullptr,nullptr,nullptr);if(rc==SQLITE_OK)return;if(rc!=SQLITE_BUSY&&rc!=SQLITE_LOCKED)throw std::runtime_error("sqlite exec");std::this_thread::yield();}}
static MR52 run_sqlite52(int total,int writers,int window){
    std::filesystem::remove("v52.sqlite");sqlite3*d=nullptr;sqlite3_open("v52.sqlite",&d);sqlite3_busy_timeout(d,30000);sqlite3_exec(d,"PRAGMA journal_mode=WAL;PRAGMA synchronous=FULL;CREATE TABLE kv(k TEXT PRIMARY KEY,v TEXT);",0,0,0);sqlite3_close(d);gnext52=0;std::atomic<uint64_t>err{0};
    double sec=timed52([&]{std::vector<std::thread>ts;for(int x=0;x<writers;++x)ts.emplace_back([&]{sqlite3*c=nullptr;sqlite3_open("v52.sqlite",&c);sqlite3_busy_timeout(c,30000);sqlite3_exec(c,"PRAGMA journal_mode=WAL;PRAGMA synchronous=FULL;",0,0,0);sqlite3_stmt*s=nullptr;sqlite3_prepare_v2(c,"INSERT INTO kv VALUES(?,?)",-1,&s,0);for(;;){std::vector<int>ids;ids.reserve(window);for(int j=0;j<window;++j){int i=gnext52.fetch_add(1);if(i>=total)break;ids.push_back(i);}if(ids.empty())break;sqlite_retry52(c,"BEGIN IMMEDIATE");for(int i:ids){auto k=K51(i),v=V51(i);sqlite3_bind_text(s,1,k.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,v.c_str(),-1,SQLITE_TRANSIENT);if(sqlite3_step(s)!=SQLITE_DONE)err++;sqlite3_reset(s);sqlite3_clear_bindings(s);}sqlite_retry52(c,"COMMIT");}sqlite3_finalize(s);sqlite3_close(c);});for(auto&t:ts)t.join();});
    sqlite3_open("v52.sqlite",&d);sqlite3_stmt*s=nullptr;sqlite3_prepare_v2(d,"SELECT v FROM kv WHERE k=?",-1,&s,0);for(int i=0;i<total;++i){auto k=K51(i);sqlite3_bind_text(s,1,k.c_str(),-1,SQLITE_TRANSIENT);if(sqlite3_step(s)!=SQLITE_ROW||std::string((const char*)sqlite3_column_text(s,0))!=V51(i))err++;sqlite3_reset(s);sqlite3_clear_bindings(s);}sqlite3_finalize(s);sqlite3_close(d);return{"SQLite",writers,window,total/sec,err.load()};
}

static MR52 run_lmdb52(int total,int writers,int window){
    std::filesystem::remove_all("v52.lmdb");std::filesystem::create_directory("v52.lmdb");MDB_env*e=nullptr;MDB_txn*t=nullptr;MDB_dbi db=0;mdb_env_create(&e);mdb_env_set_mapsize(e,1ull<<30);mdb_env_open(e,"v52.lmdb",0,0644);mdb_txn_begin(e,nullptr,0,&t);mdb_dbi_open(t,nullptr,MDB_CREATE,&db);mdb_txn_commit(t);gnext52=0;std::atomic<uint64_t>err{0};
    double sec=timed52([&]{std::vector<std::thread>ts;for(int x=0;x<writers;++x)ts.emplace_back([&]{for(;;){std::vector<int>ids;ids.reserve(window);for(int j=0;j<window;++j){int i=gnext52.fetch_add(1);if(i>=total)break;ids.push_back(i);}if(ids.empty())break;MDB_txn*tx=nullptr;if(mdb_txn_begin(e,nullptr,0,&tx)){err++;break;}for(int i:ids){auto ks=K51(i),vs=V51(i);MDB_val k{ks.size(),(void*)ks.data()},v{vs.size(),(void*)vs.data()};if(mdb_put(tx,db,&k,&v,0))err++;}if(mdb_txn_commit(tx))err++;}});for(auto&th:ts)th.join();});
    mdb_txn_begin(e,nullptr,MDB_RDONLY,&t);for(int i=0;i<total;++i){auto ks=K51(i);MDB_val k{ks.size(),(void*)ks.data()},v{};if(mdb_get(t,db,&k,&v)||std::string((char*)v.mv_data,v.mv_size)!=V51(i))err++;}mdb_txn_abort(t);mdb_dbi_close(e,db);mdb_env_close(e);return{"LMDB",writers,window,total/sec,err.load()};
}

static MR52 run_level52(int total,int writers,int window){
    std::filesystem::remove_all("v52.level");leveldb::DB*d=nullptr;leveldb::Options o;o.create_if_missing=true;if(!leveldb::DB::Open(o,"v52.level",&d).ok())throw std::runtime_error("level open");leveldb::WriteOptions wo;wo.sync=true;gnext52=0;std::atomic<uint64_t>err{0};
    double sec=timed52([&]{std::vector<std::thread>ts;for(int x=0;x<writers;++x)ts.emplace_back([&]{for(;;){leveldb::WriteBatch b;int n=0;for(;n<window;++n){int i=gnext52.fetch_add(1);if(i>=total)break;b.Put(K51(i),V51(i));}if(!n)break;if(!d->Write(wo,&b).ok())err++;}});for(auto&th:ts)th.join();});
    leveldb::ReadOptions ro;for(int i=0;i<total;++i){std::string v;if(!d->Get(ro,K51(i),&v).ok()||v!=V51(i))err++;}delete d;return{"LevelDB",writers,window,total/sec,err.load()};
}

static MR52 run_rocks52(int total,int writers,int window){
    std::filesystem::remove_all("v52.rocks");rocksdb::DB*d=nullptr;rocksdb::Options o;o.create_if_missing=true;if(!rocksdb::DB::Open(o,"v52.rocks",&d).ok())throw std::runtime_error("rocks open");rocksdb::WriteOptions wo;wo.sync=true;gnext52=0;std::atomic<uint64_t>err{0};
    double sec=timed52([&]{std::vector<std::thread>ts;for(int x=0;x<writers;++x)ts.emplace_back([&]{for(;;){rocksdb::WriteBatch b;int n=0;for(;n<window;++n){int i=gnext52.fetch_add(1);if(i>=total)break;b.Put(K51(i),V51(i));}if(!n)break;if(!d->Write(wo,&b).ok())err++;}});for(auto&th:ts)th.join();});
    rocksdb::ReadOptions ro;for(int i=0;i<total;++i){std::string v;if(!d->Get(ro,K51(i),&v).ok()||v!=V51(i))err++;}delete d;return{"RocksDB",writers,window,total/sec,err.load()};
}

int main(int argc,char**argv){
    int total=argc>1?std::stoi(argv[1]):100000;int window=argc>2?std::stoi(argv[2]):128;
    std::cout<<"engine,writers,window,total,throughput_ops_s,errors\n";int fail=0;
    for(int writers:{1,4,8,16}){
        MR52 rows[]={run_bdr52(total,writers,window),run_sqlite52(total,writers,window),run_lmdb52(total,writers,window),run_level52(total,writers,window),run_rocks52(total,writers,window)};
        for(auto&r:rows){std::cout<<r.engine<<','<<r.writers<<','<<r.window<<','<<total<<','<<r.ops<<','<<r.errors<<"\n";if(r.errors)fail++;}
    }
    return fail?2:0;
}
