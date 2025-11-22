#include "tinykv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    kv_store_t *store = NULL;

    if (kv_open(&store, ":memory:") != 0) {
        fprintf(stderr, "tiny-kv: failed to open store\n");
        return 1;
    }

    const char *key1 = "user:1";
    const char *val1 = "{\"name\":\"Alice\"}";

    if (kv_put(store, key1, val1, strlen(val1)) != 0) {
        fprintf(stderr, "tiny-kv: kv_put failed\n");
        kv_close(store);
        return 1;
    }

    void *out = NULL;
    size_t out_len = 0;

    if (kv_get(store, key1, &out, &out_len) != 0) {
        fprintf(stderr, "tiny-kv: kv_get failed\n");
        kv_close(store);
        return 1;
    }

    printf("Got value for %s: %.*s\n", key1, (int)out_len, (char *)out);
    free(out);

    /* Test delete */
    if (kv_delete(store, key1) != 0) {
        fprintf(stderr, "tiny-kv: kv_delete failed\n");
        kv_close(store);
        return 1;
    }

    /* After delete, get should fail */
    if (kv_get(store, key1, &out, &out_len) != 0) {
        printf("After delete, key '%s' is no longer present (as expected)\n", key1);
    } else {
        printf("Unexpectedly found '%s' after delete!\n", key1);
        free(out);
    }

    kv_close(store);
    return 0;
}
