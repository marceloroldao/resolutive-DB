#define main v38_original_main
#include "bdr_v38_checkpoint_bdr3.cpp"
#undef main

static void write_bytes(const fs::path&p,const std::vector<uint8_t>&b){std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write((const char*)b.data(),std::streamsize(b.size()));}
static bool rejected(const fs::path&d){try{auto r=reopen(d);(void)r;return false;}catch(...){return true;}}

int main(){
  fs::path d="v44db";fs::remove_all(d);fs::create_directory(d);size_t pass=0,total=0;
  auto snap=[&](const char*name,std::vector<uint8_t>b){++total;fs::remove_all(d);fs::create_directory(d);write_bytes(d/"snapshot.bdr3",b);if(rejected(d))++pass;std::cout<<name<<','<<rejected(d)<<"\n";};
  auto base=encode_snapshot(1,{{"K","V"}});
  {auto b=base;b[4]=0;b[5]=0;b[6]=0;b[7]=99;uint32_t c=crc_bytes(b.data(),b.size()-4);b[b.size()-4]=uint8_t(c>>24);b[b.size()-3]=uint8_t(c>>16);b[b.size()-2]=uint8_t(c>>8);b.back()=uint8_t(c);snap("snapshot_future_version",b);}
  {auto b=base;for(int i=16;i<24;++i)b[i]=0xff;uint32_t c=crc_bytes(b.data(),b.size()-4);b[b.size()-4]=uint8_t(c>>24);b[b.size()-3]=uint8_t(c>>16);b[b.size()-2]=uint8_t(c>>8);b.back()=uint8_t(c);snap("snapshot_impossible_count",b);}
  {auto b=base;size_t o=24;b[o]=0x7f;b[o+1]=0xff;b[o+2]=0xff;b[o+3]=0xff;uint32_t c=crc_bytes(b.data(),b.size()-4);b[b.size()-4]=uint8_t(c>>24);b[b.size()-3]=uint8_t(c>>16);b[b.size()-2]=uint8_t(c>>8);b.back()=uint8_t(c);snap("snapshot_huge_key",b);}
  {++total;fs::remove_all(d);fs::create_directory(d);create_wal(d/"wal-000000.log",1,1);std::fstream f(d/"wal-000000.log",std::ios::binary|std::ios::in|std::ios::out);WalHeader h{};f.read((char*)&h,sizeof(h));h.header_size=1;h.crc=whcrc(h);f.seekp(0);f.write((char*)&h,sizeof(h));f.close();if(rejected(d))++pass;std::cout<<"wal_bad_header_size,"<<rejected(d)<<"\n";}
  {++total;fs::remove_all(d);fs::create_directory(d);create_wal(d/"wal-000000.log",1,1);append_wal(d/"wal-000000.log",1,1,"K","V");std::fstream f(d/"wal-000000.log",std::ios::binary|std::ios::in|std::ios::out);f.seekp(sizeof(WalHeader)+9);uint8_t huge[4]={0x7f,0xff,0xff,0xff};f.write((char*)huge,4);f.close();if(rejected(d))++pass;std::cout<<"wal_huge_key_length,"<<rejected(d)<<"\n";}
  bool ok=pass==total;std::cout<<"summary,"<<pass<<','<<total<<','<<ok<<"\n";return ok?0:2;
}
