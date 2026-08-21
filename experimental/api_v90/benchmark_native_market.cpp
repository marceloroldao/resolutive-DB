#include "bdr/database.hpp"
#include <sqlite3.h>
#include <lmdb.h>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;
using clock_t = std::chrono::steady_clock;

static std::vector<std::pair<std::string,std::string>> make_data(size_t n){
    std::vector<std::pair<std::string,std::string>> v; v.reserve(n);
    for(size_t i=0;i<n;++i) v.emplace_back("k"+std::to_string(i), std::string(128,'A'+char(i%26)));
    return v;
}

static double secs(clock_t::time_point a, clock_t::time_point b){return std::chrono::duration<double>(b-a).count();}

static double run_bdr(const fs::path& p,const auto& data,size_t writers,size_t window){
    fs::remove_all(p); bdr::Options o; o.wal_batch=512; auto db=bdr::Database::open(p,o);
    auto t0=clock_t::now();
    std::vector<std::thread> ts;
    for(size_t w=0;w<writers;++w) ts.emplace_back([&,w]{
        std::vector<bdr::Ticket> pending; pending.reserve(window);
        for(size_t i=w;i<data.size();i+=writers){
            pending.push_back(db->put(data[i].first,data[i].second));
            if(pending.size()>=window){db->wait(pending.back()); pending.clear();}
        }
        if(!pending.empty()) db->wait(pending.back());
    });
    for(auto& t:ts)t.join(); db->sync(); auto t1=clock_t::now(); db->close();
    auto r=bdr::Database::open(p,o); for(auto& kv:data){auto x=r->get(kv.first); if(!x||*x!=kv.second) throw std::runtime_error("BDR verify failed");} r->close();
    return data.size()/secs(t0,t1);
}

static double run_sqlite(const fs::path& p,const auto& data){
    fs::remove(p); sqlite3* db=nullptr; if(sqlite3_open(p.c_str(),&db)!=SQLITE_OK) throw std::runtime_error("sqlite open");
    sqlite3_exec(db,"PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; CREATE TABLE kv(k TEXT PRIMARY KEY,v BLOB);",nullptr,nullptr,nullptr);
    sqlite3_stmt* st=nullptr; sqlite3_prepare_v2(db,"INSERT INTO kv VALUES(?,?)",-1,&st,nullptr);
    auto t0=clock_t::now(); for(auto& kv:data){sqlite3_bind_text(st,1,kv.first.data(),kv.first.size(),SQLITE_STATIC);sqlite3_bind_blob(st,2,kv.second.data(),kv.second.size(),SQLITE_STATIC); if(sqlite3_step(st)!=SQLITE_DONE) throw std::runtime_error("sqlite put"); sqlite3_reset(st); sqlite3_clear_bindings(st);} auto t1=clock_t::now();
    sqlite3_finalize(st); sqlite3_close(db); return data.size()/secs(t0,t1);
}

static double run_leveldb(const fs::path& p,const auto& data){
    fs::remove_all(p); leveldb::Options o; o.create_if_missing=true; leveldb::DB* db=nullptr; auto s=leveldb::DB::Open(o,p.string(),&db); if(!s.ok())throw std::runtime_error(s.ToString()); leveldb::WriteOptions wo; wo.sync=true;
    auto t0=clock_t::now(); for(auto&kv:data){auto x=db->Put(wo,kv.first,kv.second);if(!x.ok())throw std::runtime_error(x.ToString());} auto t1=clock_t::now(); delete db; return data.size()/secs(t0,t1);
}

static double run_rocksdb(const fs::path& p,const auto& data){
    fs::remove_all(p); rocksdb::Options o; o.create_if_missing=true; rocksdb::DB* db=nullptr; auto s=rocksdb::DB::Open(o,p.string(),&db); if(!s.ok())throw std::runtime_error(s.ToString()); rocksdb::WriteOptions wo; wo.sync=true;
    auto t0=clock_t::now(); for(auto&kv:data){auto x=db->Put(wo,kv.first,kv.second);if(!x.ok())throw std::runtime_error(x.ToString());} auto t1=clock_t::now(); delete db; return data.size()/secs(t0,t1);
}

static double run_lmdb(const fs::path& p,const auto& data){
    fs::remove_all(p); fs::create_directories(p); MDB_env* env=nullptr; mdb_env_create(&env); mdb_env_set_mapsize(env,1ull<<30); if(mdb_env_open(env,p.c_str(),0,0644)!=0)throw std::runtime_error("lmdb open");
    auto t0=clock_t::now(); for(auto&kv:data){MDB_txn* txn=nullptr; MDB_dbi dbi; mdb_txn_begin(env,nullptr,0,&txn); mdb_dbi_open(txn,nullptr,MDB_CREATE,&dbi); MDB_val k{kv.first.size(),(void*)kv.first.data()},v{kv.second.size(),(void*)kv.second.data()}; if(mdb_put(txn,dbi,&k,&v,0)!=0)throw std::runtime_error("lmdb put"); if(mdb_txn_commit(txn)!=0)throw std::runtime_error("lmdb commit");} auto t1=clock_t::now(); mdb_env_close(env); return data.size()/secs(t0,t1);
}

int main(){
    const auto data=make_data(20000); fs::create_directories("v90_out"); std::ofstream csv("v90_out/results.csv"); csv<<"engine,writers,window,ops_per_sec\n";
    for(size_t w: {1u,4u,8u,16u}) for(int rep=0;rep<3;++rep){double x=run_bdr("v90_out/bdr",data,w,128);csv<<"BDR,"<<w<<",128,"<<x<<"\n";}
    for(int rep=0;rep<3;++rep){csv<<"SQLite,1,1,"<<run_sqlite("v90_out/sqlite.db",data)<<"\n";csv<<"LMDB,1,1,"<<run_lmdb("v90_out/lmdb",data)<<"\n";csv<<"LevelDB,1,1,"<<run_leveldb("v90_out/leveldb",data)<<"\n";csv<<"RocksDB,1,1,"<<run_rocksdb("v90_out/rocksdb",data)<<"\n";}
    std::cout<<"V90 completed; see v90_out/results.csv\n";
}
