#include "bdr/bdr_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require(int ok, const char* msg) {
    if (!ok) { fprintf(stderr, "FAIL: %s (%s)\n", msg, bdr_last_error()); exit(2); }
}

int main(void) {
    system("rm -rf v87-c-db");

    bdr_options opt;
    bdr_options_init(&opt);
    opt.reserve_bytes = 8u * 1024u * 1024u;
    opt.wal_batch = 64;
    opt.partition_count = 64;

    require(bdr_abi_version() == 1u, "ABI version");

    bdr_options bad = opt;
    bad.abi_version = 99u;
    bdr_handle* h = NULL;
    require(bdr_open("v87-c-db", &bad, &h) == BDR_INCOMPATIBLE_ABI, "future ABI rejected");

    require(bdr_open("v87-c-db", &opt, &h) == BDR_OK && h, "open");

    const unsigned char key[] = {'k',0,'1'};
    const unsigned char val[] = {'v',0,'x',0,'z'};
    bdr_ticket t = 0;
    require(bdr_put(h,key,sizeof(key),val,sizeof(val),&t) == BDR_OK && t != 0, "async put");
    require(bdr_wait(h,t) == BDR_OK, "wait");
    require(bdr_durable_sequence(h) >= t, "durable sequence");

    size_t n = 0;
    require(bdr_get(h,key,sizeof(key),NULL,&n) == BDR_OK && n == sizeof(val), "get size");
    unsigned char out[16] = {0};
    size_t cap = sizeof(out);
    require(bdr_get(h,key,sizeof(key),out,&cap) == BDR_OK, "get data");
    require(cap == sizeof(val) && memcmp(out,val,sizeof(val)) == 0, "binary fidelity");

    require(bdr_put_sync(h,"plain",5,"value",5) == BDR_OK, "sync put");
    require(bdr_size(h) == 2, "size after puts");
    require(bdr_checkpoint(h) == BDR_OK, "checkpoint");
    require(bdr_close(h) == BDR_OK, "close");
    h = NULL;

    require(bdr_open("v87-c-db", &opt, &h) == BDR_OK, "reopen");
    n = 0;
    require(bdr_get(h,key,sizeof(key),NULL,&n) == BDR_OK && n == sizeof(val), "reopen binary key");
    require(bdr_delete_sync(h,key,sizeof(key)) == BDR_OK, "delete");
    n = 0;
    require(bdr_get(h,key,sizeof(key),NULL,&n) == BDR_NOT_FOUND, "not found");
    require(bdr_sync(h) == BDR_OK, "sync");
    require(bdr_last_sequence(h) == bdr_durable_sequence(h), "sequence equality");
    require(bdr_close(h) == BDR_OK, "final close");

    system("rm -rf v87-c-db");
    puts("V87_C_ABI_PASS");
    return 0;
}
