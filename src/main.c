#include "tinykv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s <db_path> put <key> <value>\n"
        "  %s <db_path> get <key>\n"
        "  %s <db_path> delete <key>\n"
        "\n"
        "Examples:\n"
        "  %s data.db put user:1 '{\"name\":\"Alice\"}'\n"
        "  %s data.db get user:1\n"
        "  %s data.db delete user:1\n",
        prog, prog, prog, prog, prog, prog
    );
}

/* Return codes for clarity */
enum {
    TINYKV_OK = 0,
    TINYKV_ERR_USAGE = 1,
    TINYKV_ERR_INTERNAL = 2,
    TINYKV_ERR_NOT_FOUND = 3
};

int main(int argc, char **argv) {
    if (argc < 3) {
        print_usage(argv[0]);
        return TINYKV_ERR_USAGE;
    }

    const char *db_path = argv[1];
    const char *cmd     = argv[2];

    (void)db_path; /* Phase 3: still in-memory, but we accept the path for future persistence */

    kv_store_t *store = NULL;
    if (kv_open(&store, db_path) != 0) {
        fprintf(stderr, "tinykv: failed to open store for '%s'\n", db_path);
        return TINYKV_ERR_INTERNAL;
    }

    int rc = TINYKV_OK;

    if (strcmp(cmd, "put") == 0) {
        if (argc < 5) {
            fprintf(stderr, "tinykv: 'put' requires <key> and <value>\n");
            print_usage(argv[0]);
            rc = TINYKV_ERR_USAGE;
            goto out;
        }

        const char *key = argv[3];
        const char *value_str = argv[4];
        size_t value_len = strlen(value_str);

        if (kv_put(store, key, value_str, value_len) != 0) {
            fprintf(stderr, "tinykv: failed to put key '%s'\n", key);
            rc = TINYKV_ERR_INTERNAL;
            goto out;
        }

        printf("OK: stored key '%s'\n", key);

    } else if (strcmp(cmd, "get") == 0) {
        if (argc < 4) {
            fprintf(stderr, "tinykv: 'get' requires <key>\n");
            print_usage(argv[0]);
            rc = TINYKV_ERR_USAGE;
            goto out;
        }

        const char *key = argv[3];
        void *value = NULL;
        size_t value_len = 0;

        if (kv_get(store, key, &value, &value_len) != 0) {
            fprintf(stderr, "tinykv: key '%s' not found\n", key);
            rc = TINYKV_ERR_NOT_FOUND;
            goto out;
        }

        /* Print raw bytes as a string; assumes value is text for now */
        printf("%.*s\n", (int)value_len, (char *)value);

        free(value);

    } else if (strcmp(cmd, "delete") == 0) {
        if (argc < 4) {
            fprintf(stderr, "tinykv: 'delete' requires <key>\n");
            print_usage(argv[0]);
            rc = TINYKV_ERR_USAGE;
            goto out;
        }

        const char *key = argv[3];

        if (kv_delete(store, key) != 0) {
            fprintf(stderr, "tinykv: failed to delete key '%s'\n", key);
            rc = TINYKV_ERR_INTERNAL;
            goto out;
        }

        printf("OK: deleted key '%s' (if it existed)\n", key);

    } else {
        fprintf(stderr, "tinykv: unknown command '%s'\n", cmd);
        print_usage(argv[0]);
        rc = TINYKV_ERR_USAGE;
        goto out;
    }

out:
    kv_close(store);
    return rc;
}
