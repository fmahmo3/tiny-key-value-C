#include "tinykv.h"

#include <stdlib.h>
#include <string.h>

typedef struct kv_entry {
    char *key;
    void *value;
    size_t value_len;
    struct kv_entry *next;
} kv_entry_t;

struct kv_store {
    kv_entry_t **buckets;
    size_t bucket_count;
};

/* Simple string hash (djb2) */
static unsigned long hash_string(const char *s) {
    unsigned long hash = 5381;
    int c;

    while ((c = (unsigned char)*s++) != 0) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

/* Choose a fixed bucket count for now. Later you can make this dynamic. */
#define TINYKV_DEFAULT_BUCKETS 1024

int kv_open(kv_store_t **out, const char *path) {
    (void)path; /* Phase 2: ignore file path, purely in-memory */

    if (!out) {
        return -1;
    }

    kv_store_t *store = malloc(sizeof *store);
    if (!store) {
        return -1;
    }

    store->bucket_count = TINYKV_DEFAULT_BUCKETS;
    store->buckets = calloc(store->bucket_count, sizeof *store->buckets);
    if (!store->buckets) {
        free(store);
        return -1;
    }

    *out = store;
    return 0;
}

/* Helper to find an entry by key (returns previous and current) */
static void find_entry(kv_store_t *store,
                       const char *key,
                       kv_entry_t ***bucket_out,
                       kv_entry_t **prev_out,
                       kv_entry_t **cur_out) {
    *bucket_out = NULL;
    *prev_out = NULL;
    *cur_out = NULL;

    if (!store || !key) {
        return;
    }

    unsigned long h = hash_string(key);
    size_t idx = h % store->bucket_count;

    kv_entry_t **bucket = &store->buckets[idx];
    kv_entry_t *prev = NULL;
    kv_entry_t *cur = *bucket;

    while (cur) {
        if (strcmp(cur->key, key) == 0) {
            *bucket_out = bucket;
            *prev_out = prev;
            *cur_out = cur;
            return;
        }
        prev = cur;
        cur = cur->next;
    }

    *bucket_out = bucket;
    *prev_out = prev;
    *cur_out = NULL; /* not found */
}

/* Allocate and copy a key string */
static char *dup_string(const char *s) {
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, s, len + 1);
    return copy;
}

int kv_put(kv_store_t *store, const char *key, const void *value, size_t value_len) {
    if (!store || !key || (!value && value_len > 0)) {
        return -1;
    }

    kv_entry_t **bucket = NULL;
    kv_entry_t *prev = NULL;
    kv_entry_t *cur = NULL;

    find_entry(store, key, &bucket, &prev, &cur);

    /* If key already exists: replace its value */
    if (cur) {
        void *new_value = NULL;

        if (value_len > 0) {
            new_value = malloc(value_len);
            if (!new_value) {
                return -1;
            }
            memcpy(new_value, value, value_len);
        }

        /* Free old value and assign new */
        free(cur->value);
        cur->value = new_value;
        cur->value_len = value_len;
        return 0;
    }

    /* Key does not exist: create new entry */
    kv_entry_t *entry = malloc(sizeof *entry);
    if (!entry) {
        return -1;
    }

    entry->key = dup_string(key);
    if (!entry->key) {
        free(entry);
        return -1;
    }

    entry->value_len = value_len;
    entry->value = NULL;

    if (value_len > 0) {
        entry->value = malloc(value_len);
        if (!entry->value) {
            free(entry->key);
            free(entry);
            return -1;
        }
        memcpy(entry->value, value, value_len);
    }

    /* Insert at the head of the bucket's linked list */
    entry->next = *bucket;
    *bucket = entry;

    return 0;
}

int kv_get(kv_store_t *store, const char *key, void **value, size_t *value_len) {
    if (!store || !key || !value || !value_len) {
        return -1;
    }

    kv_entry_t **bucket = NULL;
    kv_entry_t *prev = NULL;
    kv_entry_t *cur = NULL;

    find_entry(store, key, &bucket, &prev, &cur);

    if (!cur) {
        /* Key not found */
        return -1;
    }

    if (cur->value_len == 0) {
        *value = NULL;
        *value_len = 0;
        return 0;
    }

    void *buf = malloc(cur->value_len);
    if (!buf) {
        return -1;
    }

    memcpy(buf, cur->value, cur->value_len);
    *value = buf;
    *value_len = cur->value_len;

    return 0;
}

int kv_delete(kv_store_t *store, const char *key) {
    if (!store || !key) {
        return -1;
    }

    kv_entry_t **bucket = NULL;
    kv_entry_t *prev = NULL;
    kv_entry_t *cur = NULL;

    find_entry(store, key, &bucket, &prev, &cur);

    if (!cur) {
        /* Not found: treat as no-op, not an error */
        return 0;
    }

    if (prev) {
        prev->next = cur->next;
    } else {
        /* Deleting head of the list */
        *bucket = cur->next;
    }

    free(cur->key);
    free(cur->value);
    free(cur);

    return 0;
}

void kv_close(kv_store_t *store) {
    if (!store) {
        return;
    }

    for (size_t i = 0; i < store->bucket_count; ++i) {
        kv_entry_t *cur = store->buckets[i];
        while (cur) {
            kv_entry_t *next = cur->next;
            free(cur->key);
            free(cur->value);
            free(cur);
            cur = next;
        }
    }

    free(store->buckets);
    free(store);
}
