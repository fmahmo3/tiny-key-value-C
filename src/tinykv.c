#include "tinykv.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

typedef struct kv_entry {
    char *key;
    void *value;
    size_t value_len;
    struct kv_entry *next;
} kv_entry_t;

struct kv_store {
    kv_entry_t **buckets;
    size_t bucket_count;

    /* Persistence */
    FILE *fp;
    int is_memory_only;
};

#define TINYKV_DEFAULT_BUCKETS 1024

/* Log record operation codes */
#define TINYKV_OP_PUT    1
#define TINYKV_OP_DELETE 2

/* ---------- Hashing & internal helpers ---------- */

static unsigned long hash_string(const char *s) {
    unsigned long hash = 5381;
    int c;

    while ((c = (unsigned char)*s++) != 0) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

/* Helper to find an entry by key (returns bucket pointer, prev, and cur) */
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

/* ---------- Internal in-memory operations (no I/O) ---------- */

static int kv_put_internal(kv_store_t *store, const char *key,
                           const void *value, size_t value_len) {
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

static int kv_delete_internal(kv_store_t *store, const char *key) {
    if (!store || !key) {
        return -1;
    }

    kv_entry_t **bucket = NULL;
    kv_entry_t *prev = NULL;
    kv_entry_t *cur = NULL;

    find_entry(store, key, &bucket, &prev, &cur);

    if (!cur) {
        /* Not found: treat as no-op */
        return 0;
    }

    if (prev) {
        prev->next = cur->next;
    } else {
        *bucket = cur->next;
    }

    free(cur->key);
    free(cur->value);
    free(cur);

    return 0;
}

/* ---------- Log file helpers ---------- */

/* Write a single log record to the file (PUT or DELETE) */
static int write_log_record(FILE *fp, uint8_t op,
                            const char *key,
                            const void *value, uint32_t value_len) {
    if (!fp || !key) {
        return -1;
    }

    uint32_t key_len = (uint32_t)strlen(key);

    if (fwrite(&op, 1, 1, fp) != 1) {
        return -1;
    }
    if (fwrite(&key_len, sizeof key_len, 1, fp) != 1) {
        return -1;
    }
    if (fwrite(&value_len, sizeof value_len, 1, fp) != 1) {
        return -1;
    }
    if (fwrite(key, 1, key_len, fp) != key_len) {
        return -1;
    }
    if (value_len > 0) {
        if (fwrite(value, 1, value_len, fp) != value_len) {
            return -1;
        }
    }

    if (fflush(fp) != 0) {
        return -1;
    }

    return 0;
}

/* Replay the entire log file into the in-memory hash table */
static int replay_log(kv_store_t *store) {
    if (!store || !store->fp) {
        return 0;
    }

    if (fseek(store->fp, 0, SEEK_SET) != 0) {
        return -1;
    }

    for (;;) {
        uint8_t op;
        uint32_t key_len;
        uint32_t value_len;

        /* Try to read op; if we hit EOF cleanly, stop */
        if (fread(&op, 1, 1, store->fp) != 1) {
            break; /* EOF or partial record; treat as end of log */
        }

        if (fread(&key_len, sizeof key_len, 1, store->fp) != 1) {
            break;
        }
        if (fread(&value_len, sizeof value_len, 1, store->fp) != 1) {
            break;
        }

        char *key = malloc(key_len + 1);
        if (!key) {
            return -1;
        }
        if (fread(key, 1, key_len, store->fp) != key_len) {
            free(key);
            break;
        }
        key[key_len] = '\0';

        void *value = NULL;
        if (value_len > 0) {
            value = malloc(value_len);
            if (!value) {
                free(key);
                return -1;
            }
            if (fread(value, 1, value_len, store->fp) != value_len) {
                free(key);
                free(value);
                break;
            }
        }

        int rc = 0;
        if (op == TINYKV_OP_PUT) {
            rc = kv_put_internal(store, key, value, value_len);
        } else if (op == TINYKV_OP_DELETE) {
            rc = kv_delete_internal(store, key);
        } else {
            /* Unknown op: ignore */
        }

        free(key);
        free(value);

        if (rc != 0) {
            return rc;
        }
    }

    /* After replay, move file position to end for future appends */
    (void)fseek(store->fp, 0, SEEK_END);
    return 0;
}

/* ---------- Public API ---------- */

int kv_open(kv_store_t **out, const char *path) {
    if (!out || !path) {
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

    store->fp = NULL;
    store->is_memory_only = 0;

    /* Special case: in-memory only (no file) */
    if (strcmp(path, ":memory:") == 0) {
        store->is_memory_only = 1;
        *out = store;
        return 0;
    }

    /* File-backed store */
    FILE *fp = fopen(path, "ab+");
    if (!fp) {
        free(store->buckets);
        free(store);
        return -1;
    }

    store->fp = fp;

    /* Replay existing log to reconstruct in-memory state */
    if (replay_log(store) != 0) {
        fclose(fp);
        free(store->buckets);
        free(store);
        return -1;
    }

    *out = store;
    return 0;
}

int kv_put(kv_store_t *store, const char *key, const void *value, size_t value_len) {
    if (!store || !key || (!value && value_len > 0)) {
        return -1;
    }

    /* First append to the log (if file-backed) */
    if (!store->is_memory_only && store->fp) {
        if (write_log_record(store->fp, TINYKV_OP_PUT, key, value, (uint32_t)value_len) != 0) {
            return -1;
        }
    }

    /* Then update in-memory state */
    return kv_put_internal(store, key, value, value_len);
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
        return -1; /* not found */
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

    /* Log deletion (if file-backed). Value length is 0. */
    if (!store->is_memory_only && store->fp) {
        if (write_log_record(store->fp, TINYKV_OP_DELETE, key, NULL, 0) != 0) {
            return -1;
        }
    }

    return kv_delete_internal(store, key);
}

void kv_close(kv_store_t *store) {
    if (!store) {
        return;
    }

    if (store->fp) {
        fflush(store->fp);
        fclose(store->fp);
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
