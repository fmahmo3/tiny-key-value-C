#ifndef TINYKV_H
#define TINYKV_H

#include <stddef.h>

typedef struct kv_store kv_store_t;

/*
 * Open a key–value store at the given path.
 * Returns 0 on success, non-zero on failure.
 */
int kv_open(kv_store_t **out, const char *path);

/*
 * Store a value for a key. Overwrites if the key already exists.
 * Returns 0 on success.
 */
int kv_put(kv_store_t *store, const char *key, const void *value, size_t value_len);

/*
 * Retrieve a value for a key.
 * On success, allocates (*value) which the caller must free().
 * Returns 0 on success, non-zero if the key does not exist or on error.
 */
int kv_get(kv_store_t *store, const char *key, void **value, size_t *value_len);

/*
 * Delete a key. It is not an error if the key does not exist.
 * Returns 0 on success.
 */
int kv_delete(kv_store_t *store, const char *key);

/*
 * Flush any pending changes and close the store.
 */
void kv_close(kv_store_t *store);

#endif /* TINYKV_H */
