#define BDR_V32_NO_MAIN
#include "bdr_v32_concurrent_market.cpp"

int main(int argc,char**argv){
    if(argc<2){std::cerr<<"usage: engine [total] [writers] [window]\n";return 2;}
    std::string e=argv[1];
    int total=argc>2?std::stoi(argv[2]):1000000;
    int writers=argc>3?std::stoi(argv[3]):8;
    int window=argc>4?std::stoi(argv[4]):128;
    MR r;
    if(e=="bdr")r=run_bdr(total,writers,window);
    else if(e=="sqlite")r=run_sqlite(total,writers,window);
    else if(e=="lmdb")r=run_lmdb(total,writers,window);
    else if(e=="leveldb")r=run_level(total,writers,window);
    else if(e=="rocksdb")r=run_rocks(total,writers,window);
    else return 3;
    std::cout<<r.engine<<','<<r.writers<<','<<r.window<<','<<total<<','<<r.ops<<','<<r.errors<<"\n";
    return r.errors?4:0;
}
