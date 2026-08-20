#include "bdr/database.hpp"
#include <filesystem>
#include <iostream>
#include <map>
#include <random>
#include <string>

static std::string K(int i){return "k_"+std::to_string(i);} 
static std::string V(int r,int i){return "v_"+std::to_string(r)+"_"+std::to_string(i);} 

int main(){
    namespace fs=std::filesystem;
    const fs::path dir="v83-db"; fs::remove_all(dir);
    bdr::Options o; o.partition_count=1024; o.wal_batch=512; o.reserve_bytes=16ull<<20;
    std::map<std::string,std::string> oracle;
    std::mt19937 rng(8302026);
    constexpr int cycles=100, ops=5000, keyspace=20000;
    for(int c=0;c<cycles;++c){
        auto db=bdr::Database::open(dir,o);
        for(int j=0;j<ops;++j){
            int id=int(rng()%keyspace); auto k=K(id);
            if((rng()%5)==0){db->erase(k); oracle.erase(k);} else {auto v=V(c,id); db->put(k,v); oracle[k]=v;}
            if((j%256)==255) db->sync();
        }
        db->sync();
        if((c%5)==0) db->checkpoint();
        db->close();
        auto r=bdr::Database::open(dir,o);
        if(r->size()!=oracle.size()) {std::cerr<<"size mismatch cycle="<<c<<"\n"; return 2;}
        for(auto const& [k,v]:oracle){auto got=r->get(k); if(!got||*got!=v){std::cerr<<"value mismatch cycle="<<c<<" key="<<k<<"\n";return 3;}}
        if(r->last_sequence()!=r->durable_sequence()){std::cerr<<"sequence mismatch\n";return 4;}
        r->close();
        std::cout<<"cycle="<<c<<",records="<<oracle.size()<<"\n";
    }
    std::cout<<"V83 PASS cycles="<<cycles<<" final_records="<<oracle.size()<<"\n";
    return 0;
}
