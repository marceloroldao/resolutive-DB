#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

static uint64_t fnv1a(const void* data, size_t n, uint64_t h=1469598103934665603ULL){
  const unsigned char* p=(const unsigned char*)data;
  for(size_t i=0;i<n;++i){ h^=p[i]; h*=1099511628211ULL; }
  return h;
}
static uint64_t checksum(const std::string& k,const std::string& v,uint8_t op){
  uint64_t h=fnv1a(&op,1); h=fnv1a(k.data(),k.size(),h); return fnv1a(v.data(),v.size(),h);
}
#pragma pack(push,1)
struct RecHdr{ uint32_t magic; uint8_t op; uint32_t klen; uint32_t vlen; uint64_t csum; };
#pragma pack(pop)
static constexpr uint32_t MAGIC=0x31445242; // "BDR1" little-endian marker

class BDRPersistent {
  std::string path;
  std::unordered_map<std::string,std::string> map;
  bool append(uint8_t op,const std::string& k,const std::string& v){
    std::ofstream f(path,std::ios::binary|std::ios::app);
    if(!f) return false;
    RecHdr h{MAGIC,op,(uint32_t)k.size(),(uint32_t)v.size(),checksum(k,v,op)};
    f.write((char*)&h,sizeof(h)); f.write(k.data(),k.size()); f.write(v.data(),v.size()); f.flush();
    return (bool)f;
  }
public:
  explicit BDRPersistent(std::string p):path(std::move(p)){}
  bool load(size_t* good=nullptr,size_t* bad=nullptr){
    map.clear(); std::ifstream f(path,std::ios::binary); size_t g=0,b=0;
    if(!f){ if(good)*good=0; if(bad)*bad=0; return true; }
    for(;;){
      RecHdr h{}; f.read((char*)&h,sizeof(h));
      if(f.gcount()==0) break;
      if((size_t)f.gcount()!=sizeof(h)||h.magic!=MAGIC){ ++b; break; }
      std::string k(h.klen,'\0'),v(h.vlen,'\0');
      f.read(k.data(),h.klen); if((size_t)f.gcount()!=h.klen){ ++b; break; }
      f.read(v.data(),h.vlen); if((size_t)f.gcount()!=h.vlen){ ++b; break; }
      if(h.csum!=checksum(k,v,h.op)){ ++b; break; }
      if(h.op==1) map[k]=v; else if(h.op==2) map.erase(k); else { ++b; break; }
      ++g;
    }
    if(good)*good=g; if(bad)*bad=b; return true;
  }
  bool put(const std::string&k,const std::string&v){ if(!append(1,k,v)) return false; map[k]=v; return true; }
  bool del(const std::string&k){ if(!append(2,k,"")) return false; map.erase(k); return true; }
  bool get(const std::string&k,std::string&v)const{ auto it=map.find(k); if(it==map.end()) return false; v=it->second; return true; }
  size_t size()const{return map.size();}
};

int main(){
  const char* p="bdr_v8_test.wal"; std::remove(p);
  {
    BDRPersistent db(p); db.load();
    for(int i=0;i<100000;++i) db.put("K"+std::to_string(i),"V"+std::to_string(i));
    for(int i=0;i<10000;++i) db.put("K"+std::to_string(i),"U"+std::to_string(i));
    for(int i=20000;i<25000;++i) db.del("K"+std::to_string(i));
    std::cout<<"phase=write,size="<<db.size()<<"\n";
  }
  {
    BDRPersistent db(p); size_t good=0,bad=0;
    auto t0=std::chrono::steady_clock::now(); db.load(&good,&bad); auto t1=std::chrono::steady_clock::now();
    std::string v; int ok=0;
    ok += db.get("K1",v)&&v=="U1";
    ok += !db.get("K22000",v);
    ok += db.get("K99999",v)&&v=="V99999";
    std::cout<<"phase=reopen,size="<<db.size()<<",good="<<good<<",bad="<<bad<<",checks="<<ok
             <<",load_s="<<std::chrono::duration<double>(t1-t0).count()<<"\n";
  }
  // Simulate a torn final write by truncating bytes from the WAL tail.
  std::ifstream in(p,std::ios::binary);
  std::vector<char> data((std::istreambuf_iterator<char>(in)),{}); in.close();
  if(data.size()>17) data.resize(data.size()-17);
  std::ofstream out(p,std::ios::binary|std::ios::trunc); out.write(data.data(),data.size()); out.close();
  {
    BDRPersistent db(p); size_t good=0,bad=0; db.load(&good,&bad); std::string v;
    int ok=(db.get("K1",v)&&v=="U1") + (!db.get("K22000",v));
    std::cout<<"phase=truncated_reopen,size="<<db.size()<<",good="<<good<<",bad="<<bad<<",checks="<<ok<<"\n";
  }
}
