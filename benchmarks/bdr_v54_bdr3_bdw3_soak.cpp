#define main v48_original_main
#include "bdr_v48_bdr3_bdw3_checkpoint.cpp"
#undef main
#include <random>
#include <iomanip>

static std::string wal_name54(int n){
    char b[32];
    std::snprintf(b,sizeof(b),"wal-%06d.bdw3",n);
    return b;
}

int main(int argc,char**argv){
    const int cycles=argc>1?std::stoi(argv[1]):20;
    const int puts_per_cycle=argc>2?std::stoi(argv[2]):500;
    const int deletes_per_cycle=argc>3?std::stoi(argv[3]):100;
    constexpr uint64_t SEED=0xBDR3542026ULL;
    std::mt19937_64 rng(SEED);

    fs::path d="v54db";
    fs::remove_all(d);
    fs::create_directory(d);

    std::map<std::string,std::string> kv;
    uint64_t seq=0, puts=0, deletes=0, reopens=0, checkpoints=0;
    std::string active=wal_name54(0);
    create_wal3(d/active,1,1);
    bool all=true;

    for(int cycle=0;cycle<cycles;++cycle){
        for(int i=0;i<puts_per_cycle;++i){
            uint64_t id=rng()%200000;
            std::string k="K"+std::to_string(id);
            std::string v="C"+std::to_string(cycle)+"_"+std::to_string(i)+"_"+std::to_string(rng());
            kv[k]=v;
            append_wal3(d/active,++seq,1,k,v);
            ++puts;
        }

        for(int i=0;i<deletes_per_cycle && !kv.empty();++i){
            size_t skip=size_t(rng()%kv.size());
            auto it=kv.begin();
            std::advance(it,skip);
            std::string k=it->first;
            kv.erase(it);
            append_wal3(d/active,++seq,2,k,"");
            ++deletes;
        }

        std::string next=wal_name54(cycle+1);
        atomic_checkpoint3(d,active,next,seq,kv,uint64_t(cycle+2));
        active=next;
        ++checkpoints;

        for(int i=0;i<50;++i){
            uint64_t id=rng()%200000;
            std::string k="P"+std::to_string(cycle)+"_"+std::to_string(id);
            std::string v="Q"+std::to_string(i)+"_"+std::to_string(rng());
            kv[k]=v;
            append_wal3(d/active,++seq,1,k,v);
            ++puts;
        }

        auto r=reopen3(d);
        ++reopens;
        bool one=r.seq==seq && r.kv==kv && !fs::exists(d/"snapshot.tmp") && fs::exists(d/"snapshot.bdr3") && fs::exists(d/active);
        all=all&&one;
        if(!one){
            std::cerr<<"cycle failure "<<cycle<<" expected_seq="<<seq<<" recovered_seq="<<r.seq<<" expected_records="<<kv.size()<<" recovered_records="<<r.kv.size()<<"\n";
            break;
        }
    }

    auto final_state=reopen3(d);
    ++reopens;
    bool pass=all && final_state.seq==seq && final_state.kv==kv && !fs::exists(d/"snapshot.tmp");
    std::cout<<"seed,cycles,puts,deletes,checkpoints,reopens,final_records,last_seq,pass\n"
             <<SEED<<','<<cycles<<','<<puts<<','<<deletes<<','<<checkpoints<<','<<reopens<<','<<kv.size()<<','<<seq<<','<<pass<<"\n";
    return pass?0:2;
}
