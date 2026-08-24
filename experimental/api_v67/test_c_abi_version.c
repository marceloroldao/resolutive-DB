#include <bdr/c_api.h>

#include <stdio.h>
#include <stdlib.h>

static void fail(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(2);
}

int main(void) {
    if (bdr_abi_version() != BDR_C_ABI_VERSION) fail("ABI version mismatch");
    if (bdr_options_size() != sizeof(bdr_options)) fail("options size mismatch");

    bdr_options opt = bdr_default_options();
    if (opt.abi_version != BDR_C_ABI_VERSION) fail("default options ABI mismatch");
    if (opt.struct_size != sizeof(bdr_options)) fail("default options struct_size mismatch");

    bdr_db* db = NULL;

    bdr_options bad_version = opt;
    bad_version.abi_version += 1;
    if (bdr_open("v67-bad-version", &bad_version, &db) != BDR_INVALID_ARGUMENT)
        fail("future ABI version was not rejected");
    if (db != NULL) fail("bad ABI unexpectedly returned a handle");

    bdr_options bad_size = opt;
    bad_size.struct_size -= 1;
    if (bdr_open("v67-bad-size", &bad_size, &db) != BDR_INVALID_ARGUMENT)
        fail("wrong struct_size was not rejected");
    if (db != NULL) fail("bad struct size unexpectedly returned a handle");

    if (bdr_open("v67-good", &opt, &db) != BDR_OK || db == NULL)
        fail("valid ABI/options failed to open");
    if (bdr_close(db) != BDR_OK) fail("valid handle failed to close");

    printf("abi_version,options_size,reject_future,reject_size,valid_open,pass\n");
    printf("1,1,1,1,1,1\n");
    return 0;
}
