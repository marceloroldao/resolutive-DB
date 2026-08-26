#include "bdr/resolutive_index.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <zlib.h>

using Clock = std::chrono::steady_clock;
namespace fs = std::filesystem;

static uint32_t crc32b(const void* p, std::size_t n, uint32_t seed = 0) {
    return uint32_t(::crc32(seed, static_cast<const Bytef*>(p), uInt(n)));
}
static void put32(std::vector<uint8_t>& b, uint32_t v) { for (int i=3;i>=0;--i) b.push_back(uint8_t(v>>(8*i))); }
static void put64(std::vector<uint8_t>& b, uint64_t v) { for (int i=7;i>=0;--i) b.push_back(uint8_t(v>>(8*i))); }
static void be32_bytes(uint8_t out[4], uint32_t v) { for (int i=3;i>=0;--i) out[3-i]=uint8_t(v>>(8*i)); }
static void be64_bytes(uint8_t out[8], uint64_t v) { for (int i=7;i>=0;--i) out[7-i]=uint8_t(v>>(8*i)); }

static std::string value_for(std::size_t i) {
    std::string v(32, char('a' + (i % 26)));
    v += ":" + std::to_string(i);
    return v;
}

static std::vector<uint8_t> encode_buffered(uint64_t seq,const std::vector<std::pair<std::string,std::string>>&items){
    std::vector<uint8_t>b;b.insert(b.end(),{'B','D','R','3'});put32(b,3);put64(b,seq);put64(b,uint64_t(items.size()));
    for(const auto&[k,v]:items){put32(b,uint32_t(k.size()));put32(b,uint32_t(v.size()));b.insert(b.end(),k.begin(),k.end());b.insert(b.end(),v.begin(),v.end());}
    put32(b,crc32b(b.data(),b.size()));return b;
}

static void write_chunk(std::ofstream& out, uint32_t& crc, const void* data, std::size_t n) {
    if (!n) return;
    out.write(static_cast<const char*>(data), std::streamsize(n));
    if (!out) throw std::runtime_error("stream checkpoint write failed");
    crc = crc32b(data,n,crc);
}

static void encode_streaming(const fs::path& path,uint64_t seq,const std::vector<std::pair<std::string,std::string>>&items){
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if(!out)throw std::runtime_error("stream checkpoint open failed");
    uint32_t crc=0;
    const char magic[4]={'B','D','R','3'};write_chunk(out,crc,magic,4);
    uint8_t u32[4],u64[8];be32_bytes(u32,3);write_chunk(out,crc,u32,4);be64_bytes(u64,seq);write_chunk(out,crc,u64,8);be64_bytes(u64,uint64_t(items.size()));write_chunk(out,crc,u64,8);
    for(const auto&[k,v]:items){be32_bytes(u32,uint32_t(k.size()));write_chunk(out,crc,u32,4);be32_bytes(u32,uint32_t(v.size()));write_chunk(out,crc,u32,4);write_chunk(out,crc,k.data(),k.size());write_chunk(out,crc,v.data(),v.size());}
    be32_bytes(u32,crc);out.write(reinterpret_cast<const char*>(u32),4);if(!out)throw std::runtime_error("stream checkpoint crc write failed");out.flush();if(!out)throw std::runtime_error("stream checkpoint flush failed");
}

int main(int argc,char**argv){
    if(argc!=2)throw std::runtime_error("usage: checkpoint_streaming_ablation <buffered|streaming>");
    const std::string mode=argv[1];
    const std::size_t n=std::strtoull(std::getenv("BDR_STREAM_RECORDS")?std::getenv("BDR_STREAM_RECORDS"):"1000000",nullptr,10);
    bdr::ResolutiveIndex index(4096,.78);
    for(std::size_t i=0;i<n;++i)index.put("k"+std::to_string(i),value_for(i));
    auto items=index.snapshot_items();
    const fs::path path="checkpoint_"+mode+".bdr3";
    auto t0=Clock::now();
    if(mode=="buffered"){
        auto bytes=encode_buffered(n,items);
        std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));if(!out)throw std::runtime_error("buffered checkpoint write failed");out.flush();
    }else if(mode=="streaming"){
        encode_streaming(path,n,items);
    }else throw std::runtime_error("unknown mode");
    const double seconds=std::chrono::duration<double>(Clock::now()-t0).count();
    std::cout<<"CHECKPOINT_STREAMING_ABLATION PASS mode="<<mode<<" records="<<n<<" bytes="<<fs::file_size(path)<<" seconds="<<seconds<<"\n";
}
