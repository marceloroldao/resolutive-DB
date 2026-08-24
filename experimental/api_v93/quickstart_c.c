#include "bdr/bdr_c.h"
#include <stdio.h>
#include <string.h>

static int ok(bdr_status st, const char* what) {
    if (st == BDR_OK) return 1;
    fprintf(stderr, "%s failed: %d (%s)\n", what, (int)st, bdr_last_error());
    return 0;
}

int main(void) {
    bdr_options opt;
    bdr_options_init(&opt);

    bdr_handle* db = NULL;
    if (!ok(bdr_open("./v93_c_db", &opt, &db), "open")) return 1;

    const unsigned char key[] = {'k', 0, '1'};
    const unsigned char val[] = {'v', 0, '1'};
    bdr_ticket t = 0;
    if (!ok(bdr_put(db, key, sizeof key, val, sizeof val, &t), "put")) return 2;
    if (!ok(bdr_wait(db, t), "wait")) return 3;

    size_t need = 0;
    if (!ok(bdr_get(db, key, sizeof key, NULL, &need), "get-size")) return 4;
    unsigned char out[16] = {0};
    size_t out_len = sizeof out;
    if (!ok(bdr_get(db, key, sizeof key, out, &out_len), "get")) return 5;
    if (out_len != sizeof val || memcmp(out, val, sizeof val) != 0) return 6;

    if (!ok(bdr_checkpoint(db), "checkpoint")) return 7;
    if (!ok(bdr_close(db), "close")) return 8;

    printf("V93 C quickstart PASS\n");
    return 0;
}
