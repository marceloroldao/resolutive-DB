#define BDR_V32_NO_MAIN
#include "bdr_v32_concurrent_market.cpp"

int main(int argc,char**argv){
    int total=argc>1?std::stoi(argv[1]):1000000;
    int window=argc>2?std::stoi(argv[2]):128;
    int writers=argc>3?std::stoi(argv[3]):8;
    std::cout<<"engine,writers,window,total,throughput_ops_s,errors\n";
    MR rows[] = {
        run_bdr(total,writers,window),
        run_sqlite(total,writers,window),
        run_lmdb(total,writers,window),
        run_level(total,writers,window),
        run_rocks(total,writers,window)
    };
    for(auto&r:rows){
        std::cout<<r.engine<<','<<r.writers<<','<<r.window<<','<<total<<','<<r.ops<<','<<r.errors<<"\n";
        if(r.errors)return 2;
    }
    return 0;
}
