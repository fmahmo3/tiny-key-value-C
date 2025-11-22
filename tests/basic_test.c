#include "tinykv.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    kv_store_t *store = NULL;

    if (kv_open(&store, ":memory:") != 0) {
        fprintf(stderr, "basic_test: kv_open failed\n");
        return 1;
    }

    const char *key = "hello";
    const char *val = "world";

    if (kv_put(store, key, val, 5) != 0) {
        fprintf(stderr, "basic_test: kv_put failed\n");
        kv_close(store);
        return 1;
    }

    void *out = NULL;
    size_t out_len = 0;

    if (kv_get(store, key, &out, &out_len) != 0) {
        fprintf(stderr, "basic_test: kv_get failed\n");
        kv_close(store);
        return 1;
    }

    printf("basic_test: got value='%.*s'\n", (int)out_len, (char *)out);

    free(out);
    kv_close(store);
    return 0;
}
