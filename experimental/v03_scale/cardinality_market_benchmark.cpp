#include "bdr/database.hpp"
#include <sqlite3.h>
#include <lmdb.h>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

struct Row {
    std::string key;
    std::string value;
};

static std::vector<Row> make_data(std::size_t n) {
    std::vector<Row> data;
    data.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        data.push_back({"k" + std::to_string(i), std::string(64, char('A' + (i % 26)))});
    }
    return data;
}

static double sec(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double>(b - a).count();
}

static std::uintmax_t tree_bytes(const fs::path& p) {
    if (!fs::exists(p)) return 0;
    if (fs::is_regular_file(p)) return fs::file_size(p);
    std::uintmax_t total = 0;
    for (const auto& e : fs::recursive_directory_iterator(p)) {
        if (e.is_regular_file()) total += e.file_size();
    }
    return total;
}

struct Result {
    std::string engine;
    std::string mode;
    std::size_t records{};
    double insert_ops_s{};
    double lookup_ops_s{};
    std::uintmax_t disk_bytes{};
};

static Result run_bdr(const fs::path& p, const std::vector<Row>& data, const std::string& mode, std::size_t group) {
    fs::remove_all(p);
    bdr::Options o;
    o.wal_batch = group;
    auto db = bdr::Database::open(p, o);
    auto t0 = Clock::now();
    if (mode == "per_op") {
        for (const auto& r : data) db->put_sync(r.key, r.value);
    } else {
        std::vector<bdr::Ticket> pending;
        pending.reserve(group);
        for (const auto& r : data) {
            pending.push_back(db->put(r.key, r.value));
            if (pending.size() == group) {
                db->wait(pending.back());
                pending.clear();
            }
        }
        if (!pending.empty()) db->wait(pending.back());
        db->sync();
    }
    auto t1 = Clock::now();
    db->checkpoint();
    db->close();

    auto reopened = bdr::Database::open(p, o);
    const std::size_t samples = std::min<std::size_t>(10000, data.size());
    auto q0 = Clock::now();
    for (std::size_t i = 0; i < samples; ++i) {
        std::size_t idx = (i * 104729ull) % data.size();
        auto v = reopened->get(data[idx].key);
        if (!v || *v != data[idx].value) throw std::runtime_error("BDR lookup verify failed");
    }
    auto q1 = Clock::now();
    reopened->close();
    return {"BDR", mode, data.size(), data.size()/sec(t0,t1), samples/sec(q0,q1), tree_bytes(p)};
}

static Result run_sqlite(const fs::path& p, const std::vector<Row>& data, const std::string& mode, std::size_t group) {
    fs::remove(p);
    fs::remove(p.string() + "-wal");
    fs::remove(p.string() + "-shm");
    sqlite3* db = nullptr;
    if (sqlite3_open(p.c_str(), &db) != SQLITE_OK) throw std::runtime_error("sqlite open");
    sqlite3_exec(db, "PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; CREATE TABLE kv(k TEXT PRIMARY KEY,v BLOB);", nullptr, nullptr, nullptr);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db, "INSERT INTO kv VALUES(?,?)", -1, &st, nullptr);
    auto put_one = [&](const Row& r) {
        sqlite3_bind_text(st, 1, r.key.data(), int(r.key.size()), SQLITE_STATIC);
        sqlite3_bind_blob(st, 2, r.value.data(), int(r.value.size()), SQLITE_STATIC);
        if (sqlite3_step(st) != SQLITE_DONE) throw std::runtime_error("sqlite insert");
        sqlite3_reset(st); sqlite3_clear_bindings(st);
    };
    auto t0 = Clock::now();
    if (mode == "per_op") {
        for (const auto& r : data) put_one(r);
    } else {
        for (std::size_t base = 0; base < data.size(); base += group) {
            sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
            std::size_t end = std::min(data.size(), base + group);
            for (std::size_t i = base; i < end; ++i) put_one(data[i]);
            sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
        }
    }
    auto t1 = Clock::now();
    sqlite3_finalize(st);
    sqlite3_wal_checkpoint_v2(db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr);

    sqlite3_stmt* q = nullptr;
    sqlite3_prepare_v2(db, "SELECT v FROM kv WHERE k=?", -1, &q, nullptr);
    const std::size_t samples = std::min<std::size_t>(10000, data.size());
    auto q0 = Clock::now();
    for (std::size_t i = 0; i < samples; ++i) {
        std::size_t idx = (i * 104729ull) % data.size();
        sqlite3_bind_text(q, 1, data[idx].key.data(), int(data[idx].key.size()), SQLITE_STATIC);
        if (sqlite3_step(q) != SQLITE_ROW) throw std::runtime_error("sqlite lookup");
        const void* blob = sqlite3_column_blob(q, 0);
        int bytes = sqlite3_column_bytes(q, 0);
        if (bytes != int(data[idx].value.size()) || std::string(static_cast<const char*>(blob), bytes) != data[idx].value) throw std::runtime_error("sqlite verify");
        sqlite3_reset(q); sqlite3_clear_bindings(q);
    }
    auto q1 = Clock::now();
    sqlite3_finalize(q); sqlite3_close(db);
    return {"SQLite", mode, data.size(), data.size()/sec(t0,t1), samples/sec(q0,q1), tree_bytes(p)};
}

static Result run_lmdb(const fs::path& p, const std::vector<Row>& data, const std::string& mode, std::size_t group) {
    fs::remove_all(p); fs::create_directories(p);
    MDB_env* env = nullptr; mdb_env_create(&env); mdb_env_set_mapsize(env, 8ull<<30);
    if (mdb_env_open(env, p.c_str(), 0, 0644) != 0) throw std::runtime_error("lmdb open");
    auto write_range = [&](std::size_t begin, std::size_t end) {
        MDB_txn* txn = nullptr; MDB_dbi dbi;
        if (mdb_txn_begin(env, nullptr, 0, &txn) != 0) throw std::runtime_error("lmdb txn");
        if (mdb_dbi_open(txn, nullptr, MDB_CREATE, &dbi) != 0) throw std::runtime_error("lmdb dbi");
        for (std::size_t i = begin; i < end; ++i) {
            MDB_val k{data[i].key.size(), const_cast<char*>(data[i].key.data())};
            MDB_val v{data[i].value.size(), const_cast<char*>(data[i].value.data())};
            if (mdb_put(txn, dbi, &k, &v, 0) != 0) throw std::runtime_error("lmdb put");
        }
        if (mdb_txn_commit(txn) != 0) throw std::runtime_error("lmdb commit");
    };
    auto t0 = Clock::now();
    if (mode == "per_op") {
        for (std::size_t i = 0; i < data.size(); ++i) write_range(i, i+1);
    } else {
        for (std::size_t base = 0; base < data.size(); base += group) write_range(base, std::min(data.size(), base+group));
    }
    auto t1 = Clock::now();

    MDB_txn* rtxn = nullptr; MDB_dbi dbi; mdb_txn_begin(env, nullptr, MDB_RDONLY, &rtxn); mdb_dbi_open(rtxn, nullptr, 0, &dbi);
    const std::size_t samples = std::min<std::size_t>(10000, data.size());
    auto q0 = Clock::now();
    for (std::size_t i = 0; i < samples; ++i) {
        std::size_t idx = (i * 104729ull) % data.size();
        MDB_val k{data[idx].key.size(), const_cast<char*>(data[idx].key.data())}, v{};
        if (mdb_get(rtxn, dbi, &k, &v) != 0 || std::string(static_cast<char*>(v.mv_data), v.mv_size) != data[idx].value) throw std::runtime_error("lmdb verify");
    }
    auto q1 = Clock::now();
    mdb_txn_abort(rtxn); mdb_env_sync(env, 1); mdb_env_close(env);
    return {"LMDB", mode, data.size(), data.size()/sec(t0,t1), samples/sec(q0,q1), tree_bytes(p)};
}

static Result run_leveldb(const fs::path& p, const std::vector<Row>& data, const std::string& mode, std::size_t group) {
    fs::remove_all(p); leveldb::Options o; o.create_if_missing = true; leveldb::DB* db = nullptr;
    auto s = leveldb::DB::Open(o, p.string(), &db); if (!s.ok()) throw std::runtime_error(s.ToString());
    leveldb::WriteOptions wo; wo.sync = true;
    auto t0 = Clock::now();
    if (mode == "per_op") {
        for (const auto& r : data) { auto x = db->Put(wo, r.key, r.value); if (!x.ok()) throw std::runtime_error(x.ToString()); }
    } else {
        for (std::size_t base=0;base<data.size();base+=group) {
            leveldb::WriteBatch batch; std::size_t end=std::min(data.size(),base+group);
            for(std::size_t i=base;i<end;++i) batch.Put(data[i].key,data[i].value);
            auto x=db->Write(wo,&batch); if(!x.ok()) throw std::runtime_error(x.ToString());
        }
    }
    auto t1 = Clock::now();
    const std::size_t samples=std::min<std::size_t>(10000,data.size()); std::string out;
    auto q0=Clock::now();
    for(std::size_t i=0;i<samples;++i){std::size_t idx=(i*104729ull)%data.size(); auto x=db->Get(leveldb::ReadOptions{},data[idx].key,&out); if(!x.ok()||out!=data[idx].value)throw std::runtime_error("leveldb verify");}
    auto q1=Clock::now(); delete db;
    return {"LevelDB",mode,data.size(),data.size()/sec(t0,t1),samples/sec(q0,q1),tree_bytes(p)};
}

static Result run_rocksdb(const fs::path& p, const std::vector<Row>& data, const std::string& mode, std::size_t group) {
    fs::remove_all(p); rocksdb::Options o; o.create_if_missing=true; rocksdb::DB* db=nullptr;
    auto s=rocksdb::DB::Open(o,p.string(),&db); if(!s.ok())throw std::runtime_error(s.ToString()); rocksdb::WriteOptions wo; wo.sync=true;
    auto t0=Clock::now();
    if(mode=="per_op"){
        for(const auto&r:data){auto x=db->Put(wo,r.key,r.value);if(!x.ok())throw std::runtime_error(x.ToString());}
    } else {
        for(std::size_t base=0;base<data.size();base+=group){rocksdb::WriteBatch batch;std::size_t end=std::min(data.size(),base+group);for(std::size_t i=base;i<end;++i)batch.Put(data[i].key,data[i].value);auto x=db->Write(wo,&batch);if(!x.ok())throw std::runtime_error(x.ToString());}
    }
    auto t1=Clock::now();
    const std::size_t samples=std::min<std::size_t>(10000,data.size());std::string out;auto q0=Clock::now();
    for(std::size_t i=0;i<samples;++i){std::size_t idx=(i*104729ull)%data.size();auto x=db->Get(rocksdb::ReadOptions{},data[idx].key,&out);if(!x.ok()||out!=data[idx].value)throw std::runtime_error("rocksdb verify");}
    auto q1=Clock::now(); delete db;
    return {"RocksDB",mode,data.size(),data.size()/sec(t0,t1),samples/sec(q0,q1),tree_bytes(p)};
}

int main() {
    const std::string mode = std::getenv("BDR_MARKET_MODE") ? std::getenv("BDR_MARKET_MODE") : "grouped";
    const std::size_t n = std::getenv("BDR_MARKET_RECORDS") ? std::stoull(std::getenv("BDR_MARKET_RECORDS")) : 1000000;
    const std::size_t group = 512;
    auto data = make_data(n);
    fs::create_directories("v03_market_out");
    std::vector<Result> rs;
    rs.push_back(run_bdr("v03_market_out/bdr",data,mode,group));
    rs.push_back(run_sqlite("v03_market_out/sqlite.db",data,mode,group));
    rs.push_back(run_lmdb("v03_market_out/lmdb",data,mode,group));
    rs.push_back(run_leveldb("v03_market_out/leveldb",data,mode,group));
    rs.push_back(run_rocksdb("v03_market_out/rocksdb",data,mode,group));
    std::cout << "engine,mode,records,insert_ops_per_sec,lookup_ops_per_sec,disk_bytes,bytes_per_record\n";
    for (const auto& r : rs) {
        std::cout << r.engine << ',' << r.mode << ',' << r.records << ',' << r.insert_ops_s << ',' << r.lookup_ops_s << ',' << r.disk_bytes << ',' << (double(r.disk_bytes)/r.records) << '\n';
    }
}
