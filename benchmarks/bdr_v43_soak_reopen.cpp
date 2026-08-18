#define main v41_original_main
#include "bdr_v41_delete_multicheckpoint.cpp"
#undef main
#include <random>

int main(){
  constexpr uint64_t SEED=0xB0D43026ULL;std::mt19937_64 rng(SEED);fs::path d="v43db";fs::remove_all(d);fs::create_directory(d);
  std::map<std::string,std::string>kv;uint64_t seq=0;std::string active="wal-000000.log";create_wal(d/active,1,1);uint64_t puts=0,dels=0,reopens=0;bool all=true;
  for(int cycle=0;cycle<20;++cycle){
    for(int i=0;i<2000;++i){uint64_t id=rng()%200000;std::string k="K"+std::to_string(id),v="C"+std::to_string(cycle)+"_"+std::to_string(i);kv[k]=v;append_wal(d/active,++seq,1,k,v);++puts;}
    for(int i=0;i<500;++i){if(kv.empty())break;size_t skip=size_t(rng()%kv.size());auto it=kv.begin();std::advance(it,skip);std::string k=it->first;kv.erase(it);append_wal(d/active,++seq,2,k,"");++dels;}
    std::string next="wal-"+std::string(6-std::to_string(cycle+1).size(),'0')+std::to_string(cycle+1)+".log";
    checkpoint_gen(d,active,next,seq,kv,uint64_t(cycle+2));active=next;
    for(int i=0;i<100;++i){uint64_t id=rng()%200000;std::string k="P"+std::to_string(cycle)+"_"+std::to_string(id),v="Q"+std::to_string(i);kv[k]=v;append_wal(d/active,++seq,1,k,v);++puts;}
    auto r=reopen(d);++reopens;bool ok=r.seq==seq&&r.kv==kv&&!fs::exists(d/"snapshot.tmp");all=all&&ok;if(!ok)break;
  }
  auto r=reopen(d);++reopens;bool pass=all&&r.seq==seq&&r.kv==kv;
  std::cout<<"seed,cycles,puts,deletes,reopens,final_records,last_seq,pass\n"<<SEED<<",20,"<<puts<<','<<dels<<','<<reopens<<','<<kv.size()<<','<<seq<<','<<pass<<"\n";
  return pass?0:2;
}
