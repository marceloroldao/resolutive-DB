#include "bdr/database.hpp"

#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
static void req(bool c,const char*m){if(!c)throw std::runtime_error(m);} 
static fs::path active_wal(const fs::path&d){fs::path p;for(const auto&e:fs::directory_iterator(d))if(e.path().extension()==".bdw3"&&(p.empty()||e.path().filename()>p.filename()))p=e.path();if(p.empty())throw std::runtime_error("no wal");return p;}

int main(){
    fs::path dir="v56-api-test-db";fs::remove_all(dir);
    bdr::Options o;o.partition_count=64;o.partition_max_load=0.72;o.wal_batch=128;o.reserve_bytes=16ull*1024ull*1024ull;
    uint64_t durable_before_tail=0;
    {
        auto db=bdr::Database::open(dir,o);
        for(int i=0;i<20000;++i){auto t=db->put("K"+std::to_string(i),"V"+std::to_string(i));if((i&255)==255)db->wait(t);}
        db->sync();
        req(db->size()==20000,"size after put");
        for(int i=0;i<20000;i+=137)req(db->get("K"+std::to_string(i)).value_or("")=="V"+std::to_string(i),"lookup mismatch");

        // Updates must not change cardinality.
        for(int i=0;i<1000;++i)db->put_sync("K"+std::to_string(i),"U"+std::to_string(i));
        req(db->size()==20000,"update changed size");

        // Deletes force backward-shift inside local Robin Hood tables.
        for(int i=0;i<5000;++i)db->erase_sync("K"+std::to_string(i*2));
        req(db->size()==15000,"delete size mismatch");
        for(int i=0;i<5000;++i)req(!db->contains("K"+std::to_string(i*2)),"deleted key survived");

        auto st=db->index_stats();
        req(st.partitions==64,"partition count mismatch");
        req(st.records==15000,"stats record mismatch");
        req(st.slots>st.records,"Robin Hood did not allocate slots");
        req(st.max_partition_records>0,"partition stats empty");

        db->checkpoint();
        db->put_sync("after-checkpoint","OK");
        durable_before_tail=db->durable_sequence();
        db->close();
    }
    {
        auto db=bdr::Database::open(dir,o);
        req(db->size()==15001,"reopen size mismatch");
        req(db->get("after-checkpoint").value_or("")=="OK","post-checkpoint key missing");
        req(db->durable_sequence()==durable_before_tail,"reopen durable frontier mismatch");
        db->close();
    }

    // Append an unconfirmed partial frame. Recovery must trim it and retain every durable ticket.
    auto wal=active_wal(dir);int fd=::open(wal.c_str(),O_WRONLY|O_APPEND);req(fd>=0,"open tail");const unsigned char partial[3]={0x00,0x00,0x01};req(::write(fd,partial,sizeof(partial))==3,"append partial");::close(fd);
    {
        auto db=bdr::Database::open(dir,o);
        req(db->durable_sequence()==durable_before_tail,"torn tail changed durable frontier");
        req(db->get("after-checkpoint").value_or("")=="OK","valid prefix lost");
        db->put_sync("after-repair","OK2");
        db->checkpoint();
        db->close();
    }
    {
        auto db=bdr::Database::open(dir,o);
        req(db->get("after-repair").value_or("")=="OK2","repair continuation failed");
        req(db->size()==15002,"final size mismatch");
        db->close();
    }

    std::cout<<"records,partitions,resize,update,delete,checkpoint,reopen,torn_tail,post_repair,pass\n"
             <<"15002,64,1,1,1,1,1,1,1,1\n";
    return 0;
}
