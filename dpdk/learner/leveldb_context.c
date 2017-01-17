#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "leveldb_context.h"

char TEST_DB[] = "/tmp/libpaxos";

enum boolean {
    false,
    true
};

struct leveldb_ctx* new_leveldb_context(void) {
    struct leveldb_ctx *ctx = malloc(sizeof (struct leveldb_ctx));
    ctx->options = leveldb_options_create();
    ctx->woptions = leveldb_writeoptions_create();
    ctx->roptions = leveldb_readoptions_create();
    open_db(ctx, TEST_DB);
    return ctx;
}


void open_db(struct leveldb_ctx *ctx, char* db_name) {
    char *err = NULL;
    leveldb_options_set_create_if_missing(ctx->options, true);
    ctx->db = leveldb_open(ctx->options, db_name, &err);
    if (err != NULL) {
        fprintf(stderr, "Open fail.\n");
        exit (EXIT_FAILURE);
    }
    leveldb_free(err); err = NULL;
}

void destroy_db(struct leveldb_ctx *ctx, char* db_name) {
    char *err = NULL;
    leveldb_destroy_db(ctx->options, db_name, &err);
    if (err != NULL) {
        fprintf(stderr, "Open fail.\n");
        exit (EXIT_FAILURE);
    }
    leveldb_free(err); err = NULL;
}

int add_entry(struct leveldb_ctx *ctx, int sync, char *key, int ksize, char* val, int vsize) {
    char *err = NULL;
    leveldb_writeoptions_set_sync(ctx->woptions, sync);
    leveldb_put(ctx->db, ctx->woptions, key, ksize, val, vsize, &err);
    if (err != NULL) {
        fprintf(stderr, "Write fail.\n");
        return(1);
    }
    leveldb_free(err); err = NULL;
    return 0;
}

int get_value(struct leveldb_ctx *ctx, char *key, size_t ksize, char** val, size_t* vsize) {
    char *err = NULL;
    *val = leveldb_get(ctx->db, ctx->roptions, key, ksize, vsize, &err);
    if (err != NULL) {
        fprintf(stderr, "Read fail.\n");
        return(1);
    }
    leveldb_free(err); err = NULL;
    return 0;
}

int delete_entry(struct leveldb_ctx *ctx, char *key, int ksize) {
    char *err = NULL;
    leveldb_delete(ctx->db, ctx->woptions, key, ksize, &err);
    if (err != NULL) {
        fprintf(stderr, "Delete fail.\n");
        return(1);
    }
    leveldb_free(err); err = NULL;
    return 0;
}

void free_leveldb_context(struct leveldb_ctx *ctx) {
    leveldb_close(ctx->db);
    leveldb_writeoptions_destroy(ctx->woptions);
    leveldb_readoptions_destroy(ctx->roptions);
    leveldb_options_destroy(ctx->options);
    free(ctx);
}


