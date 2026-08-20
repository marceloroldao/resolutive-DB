#include <bdr/c_api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_status(bdr_status got, bdr_status want, const char* what) {
    if (got != want) {
        fprintf(stderr, "%s failed: got=%d want=%d err=%s\n", what, (int)got, (int)want, bdr_last_error());
        exit(2);
    }
}

int main(void) {
    const char* path = "v61-c-api-db";
    system("rm -rf v61-c-api-db");

    bdr_options opt = bdr_default_options();
    opt.reserve_bytes = 8u * 1024u * 1024u;
    opt.wal_batch = 64;
    opt.rho_partitions = 64;

    bdr_db* db = NULL;
    require_status(bdr_open(path, &opt, &db), BDR_OK, "open");

    const unsigned char key1[] = {'k', 0, '1'};
    const unsigned char val1[] = {'v', 0, 'a', 'l'};
    uint64_t ticket = 0;
    require_status(bdr_put(db, key1, sizeof(key1), val1, sizeof(val1), &ticket), BDR_OK, "put");
    require_status(bdr_wait(db, ticket), BDR_OK, "wait");

    size_t need = 0;
    require_status(bdr_get(db, key1, sizeof(key1), NULL, 0, &need), BDR_BUFFER_TOO_SMALL, "get-size");
    if (need != sizeof(val1)) return 3;

    unsigned char out[16] = {0};
    require_status(bdr_get(db, key1, sizeof(key1), out, sizeof(out), &need), BDR_OK, "get");
    if (need != sizeof(val1) || memcmp(out, val1, sizeof(val1)) != 0) return 4;

    const char key2[] = "beta";
    const char val2[] = "B";
    require_status(bdr_put_sync(db, key2, 4, val2, 1), BDR_OK, "put_sync");
    require_status(bdr_delete_sync(db, key1, sizeof(key1)), BDR_OK, "delete_sync");
    require_status(bdr_checkpoint(db), BDR_OK, "checkpoint");

    size_t sz = 0;
    require_status(bdr_size(db, &sz), BDR_OK, "size");
    if (sz != 1) return 5;
    require_status(bdr_close(db), BDR_OK, "close");

    db = NULL;
    require_status(bdr_open(path, &opt, &db), BDR_OK, "reopen");
    need = 0;
    require_status(bdr_get(db, key1, sizeof(key1), out, sizeof(out), &need), BDR_NOT_FOUND, "deleted-key");
    require_status(bdr_get(db, key2, 4, out, sizeof(out), &need), BDR_OK, "beta-after-reopen");
    if (need != 1 || out[0] != 'B') return 6;

    uint64_t last = 0, durable = 0;
    require_status(bdr_last_sequence(db, &last), BDR_OK, "last-sequence");
    require_status(bdr_durable_sequence(db, &durable), BDR_OK, "durable-sequence");
    if (last != durable) return 7;

    require_status(bdr_close(db), BDR_OK, "final-close");

    printf("binary_key,binary_value,ticket_wait,get_buffer_protocol,delete,checkpoint,reopen,sequence,pass\n");
    printf("1,1,1,1,1,1,1,1,1\n");
    return 0;
}
