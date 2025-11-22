#include "tinykv.h"
#include <stdio.h>

int main(void) {
    kv_store_t *store = NULL;

    if (kv_open(&store, "data.db") != 0) {
        fprintf(stderr, "Failed to open store\n");
        return 1;
    }

    printf("tiny-kv initialized (stub)\n");

    kv_close(store);
    return 0;
}
