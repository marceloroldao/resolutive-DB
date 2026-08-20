#define main v45_original_main
#include "bdr_v45_bdw3_framing.cpp"
#undef main
#include <map>
#include <stdexcept>

struct ReplayResult{std::map<std::string,std::string> kv;uint64_t last_seq=0;size_t good=0;R status=R::OK;};
static ReplayResult replay(const fs::path&p){ReplayResult out;std::ifstream f(p,std::ios::binary);W3H wh{};f.read((char*)&wh,sizeof(wh));if(f.gcount()!=sizeof(wh)||std::memcmp(wh.magic,"BDW3",4)||wh.version!=3||wh.hsize!=sizeof(wh)||hcrc(wh)!=wh.crc){out.status=R::CORRUPT;return out;}uint64_t expected=wh.first_seq;for(;;){uint8_t lb[4];f.read((char*)lb,4);if(f.gcount()==0){out.status=R::OK;return out;}if(f.gcount()!=4){out.status=R::TORN;return out;}uint32_t total=be32(lb);constexpr uint32_t MIN=4+8+1+4+4+4+4;if(total<MIN||total>(1u<<24)){out.status=R::CORRUPT;return out;}std::vector<uint8_t>b(total-4);f.read((char*)b.data(),std::streamsize(b.size()));if(f.gcount()!=std::streamsize(b.size())){out.status=R::TORN;return out;}const uint8_t*q=b.data();uint64_t seq=be64(q);q+=8;uint8_t op=*q++;uint32_t kl=be32(q);q+=4;uint32_t vl=be32(q);q+=4;uint32_t hc=be32(q);q+=4;std::vector<uint8_t>hh;put64(hh,seq);hh.push_back(op);put32(hh,kl);put32(hh,vl);if(crc(hh.data(),hh.size())!=hc||seq!=expected||(op!=1&&op!=2)){out.status=R::CORRUPT;return out;}if(uint64_t(kl)+vl+4!=uint64_t(b.data()+b.size()-q)){out.status=R::CORRUPT;return out;}uint32_t got=be32(b.data()+b.size()-4);std::vector<uint8_t>whole;whole.insert(whole.end(),lb,lb+4);whole.insert(whole.end(),b.begin(),b.end()-4);if(crc(whole.data(),whole.size())!=got){out.status=R::CORRUPT;return out;}std::string k((const char*)q,kl);q+=kl;std::string v((const char*)q,vl);if(op==1)out.kv[k]=v;else out.kv.erase(k);out.last_seq=seq;++out.good;++expected;}}

int main(){fs::path p="v46.bdw3";create_wal(p);std::map<std::string,std::string>want;uint64_t seq=0;for(int i=0;i<5000;++i){std::string k="K"+std::to_string(i),v="V"+std::to_string(i);append(p,++seq,1,k,v);want[k]=v;}for(int i=0;i<1000;++i){std::string k="K"+std::to_string(i*3);append(p,++seq,2,k,"");want.erase(k);}auto clean=replay(p);bool clean_ok=clean.status==R::OK&&clean.last_seq==seq&&clean.kv==want&&clean.good==6000;

fs::copy_file(p,"v46_torn.bdw3",fs::copy_options::overwrite_existing);auto sz=fs::file_size("v46_torn.bdw3");fs::resize_file("v46_torn.bdw3",sz-7);auto torn=replay("v46_torn.bdw3");bool torn_ok=torn.status==R::TORN&&torn.good==5999&&torn.last_seq==5999;

fs::copy_file(p,"v46_corrupt.bdw3",fs::copy_options::overwrite_existing);flip("v46_corrupt.bdw3",sizeof(W3H)+20);auto bad=replay("v46_corrupt.bdw3");bool corrupt_ok=bad.status==R::CORRUPT&&bad.good==0;

bool pass=clean_ok&&torn_ok&&corrupt_ok;std::cout<<"clean_ok,torn_ok,corrupt_ok,clean_good,torn_good,clean_last_seq,torn_last_seq,pass\n"<<clean_ok<<','<<torn_ok<<','<<corrupt_ok<<','<<clean.good<<','<<torn.good<<','<<clean.last_seq<<','<<torn.last_seq<<','<<pass<<"\n";return pass?0:2;}
