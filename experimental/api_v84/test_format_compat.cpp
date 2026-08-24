#include "bdr/database.hpp"
#include <filesystem>
#include <iostream>
#include <string>

static std::string K(int i){return "compat_k_"+std::to_string(i);} 
static std::string A(int i){return "A_"+std::to_string(i);} 
static std::string B(int i){return "B_"+std::to_string(i);} 

int main(int argc,char**argv){
    if(argc!=3){std::cerr<<"usage: compat <create|upgrade|verify> <dir>\n";return 64;}
    std::filesystem::path dir=argv[2]; bdr::Options o; o.partition_count=256; o.wal_batch=128; o.reserve_bytes=4ull<<20;
    std::string mode=argv[1];
    if(mode=="create"){
        std::filesystem::remove_all(dir); auto db=bdr::Database::open(dir,o);
        for(int i=0;i<5000;++i) db->put(K(i),A(i)); db->sync(); db->checkpoint(); db->close(); return 0;
    }
    auto db=bdr::Database::open(dir,o);
    for(int i=0;i<5000;++i){auto v=db->get(K(i)); if(!v||(*v!=A(i)&&*v!=B(i))){std::cerr<<"bad base "<<i<<"\n";return 2;}}
    if(mode=="upgrade"){
        for(int i=0;i<5000;i+=2) db->put(K(i),B(i));
        for(int i=5000;i<7500;++i) db->put(K(i),B(i));
        db->sync(); db->checkpoint(); db->close(); return 0;
    }
    if(mode=="verify"){
        for(int i=0;i<5000;++i){auto v=db->get(K(i));auto exp=(i%2==0)?B(i):A(i);if(!v||*v!=exp){std::cerr<<"bad upgraded "<<i<<"\n";return 3;}}
        for(int i=5000;i<7500;++i){auto v=db->get(K(i));if(!v||*v!=B(i)){std::cerr<<"bad added "<<i<<"\n";return 4;}}
        if(db->last_sequence()!=db->durable_sequence()){std::cerr<<"sequence mismatch\n";return 5;}
        db->close(); std::cout<<"V84 PASS\n"; return 0;
    }
    return 65;
}
