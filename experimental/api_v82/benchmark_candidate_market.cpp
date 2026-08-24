#include "bdr/database.hpp"

// Reuse the exact V52 competitor implementations and key/value generators,
// but hide its historical BDR implementation and main entrypoint.
#define main v52_historical_main_unused
#define run_bdr52 run_bdr52_historical_unused
#include "../../benchmarks/bdr_v52_bdw3_market.cpp"
#undef run_bdr52
#undef main

static MR52 run_bdr82(int total,int writers,int window){
    const std::filesystem::path root="v82.bdr";
    std::filesystem::remove_all(root);
    gnext52=0;
    std::atomic<uint64_t>err{0};

    bdr::Options opt;
    opt.reserve_bytes=std::max<std::size_t>(64ull*1024ull*1024ull,std::size_t(total)*128ull);
    opt.wal_batch=512;
    opt.partition_count=4096;
    opt.partition_max_load=0.78;
    opt.keep_size_preallocation=true;

    double sec=timed52([&]{
        auto db=bdr::Database::open(root,opt);
        std::vector<std::thread>ts;
        for(int x=0;x<writers;++x)ts.emplace_back([&]{
            bdr::Ticket last{};
            int pending=0;
            for(;;){
                int i=gnext52.fetch_add(1);
                if(i>=total)break;
                try{
                    last=db->put(K51(i),V51(i));
                    if(++pending>=window){db->wait(last);pending=0;}
                }catch(...){err++;break;}
            }
            if(pending&&last){try{db->wait(last);}catch(...){err++;}}
        });
        for(auto&t:ts)t.join();
        try{
            db->sync();
            if(db->size()!=std::size_t(total)||db->last_sequence()!=uint64_t(total)||
               db->durable_sequence()!=uint64_t(total))err++;
            db->close();
        }catch(...){err++;}
    });

    try{
        auto db=bdr::Database::open(root,opt);
        if(db->size()!=std::size_t(total)||db->last_sequence()!=uint64_t(total)||
           db->durable_sequence()!=uint64_t(total))err++;
        for(int i=0;i<total;++i){
            auto v=db->get(K51(i));
            if(!v||*v!=V51(i))err++;
        }
        db->close();
    }catch(...){err++;}

    std::filesystem::remove_all(root);
    return{"BDR-V80-candidate",writers,window,total/sec,err.load()};
}

int main(int argc,char**argv){
    int total=argc>1?std::stoi(argv[1]):100000;
    int window=argc>2?std::stoi(argv[2]):128;
    std::cout<<"engine,writers,window,total,throughput_ops_s,errors\n";
    int fail=0;
    for(int writers:{1,4,8,16}){
        MR52 rows[]={
            run_bdr82(total,writers,window),
            run_sqlite52(total,writers,window),
            run_lmdb52(total,writers,window),
            run_level52(total,writers,window),
            run_rocks52(total,writers,window)
        };
        for(auto&r:rows){
            std::cout<<r.engine<<','<<r.writers<<','<<r.window<<','<<total<<','<<r.ops<<','<<r.errors<<"\n";
            if(r.errors)fail++;
        }
    }
    return fail?2:0;
}
