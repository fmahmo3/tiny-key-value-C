#include "tinykv.h"
#include <stdlib.h>

struct kv_store {
    int placeholder;
};

int kv_open(kv_store_t **out, const char *path) {
    (void)path;

    kv_store_t *store = malloc(sizeof *store);
    if (!store) {
        return -1;
    }
    store->placeholder = 0;
    *out = store;
    return 0;
}

int kv_put(kv_store_t *store, const char *key, const void *value, size_t value_len) {
    (void)store;
    (void)key;
    (void)value;
    (void)value_len;
    /* TODO: implement real storage */
    return 0;
}

int kv_get(kv_store_t *store, const char *key, void **value, size_t *value_len) {
    (void)store;
    (void)key;
    (void)value;
    (void)value_len;
    /* TODO: implement real lookup */
    return -1; /* not found */
}

int kv_delete(kv_store_t *store, const char *key) {
    (void)store;
    (void)key;
    /* TODO: implement real delete (tombstone) */
    return 0;
}

void kv_close(kv_store_t *store) {
    if (!store) return;
    free(store);
}
