#include "context.h"
#include <string.h>

void context_init_errtable(CompilerContext *ctx) {
    // Inject ErrNull, ErrDivisionByZero, and others
    int null_id = ctx->next_error_id++;
    hashmap_put(&ctx->error_table, "ErrNull", (void*)(intptr_t)(++null_id));
    hashmap_put(&ctx->error_table, "ErrDivisionByZero", (void*)(intptr_t)(++null_id));
}

void context_init(CompilerContext *ctx, Arena *arena) {
    if (!ctx) return;

    ctx->arena = arena;
    ctx->error_count = 0;
    ctx->lexer_error_count = 0;
    ctx->parser_error_count = 0;
    ctx->semantic_error_count = 0;
    ctx->alir_error_count = 0;

    // Initialize diagnostic state
    // Default namespace is "main"
    strncpy(ctx->current_namespace, "main", 255);
    ctx->current_namespace[255] = '\0';

    ctx->last_reported_namespace[0] = '\0';
    ctx->last_reported_filename[0] = '\0';

    // Initialize the global String Interning Pool
    // 1024 is a good starting capacity for average source files
    hashmap_init(&ctx->string_pool, arena, 1024);
    hashmap_init(&ctx->error_table, arena, 64);
    hashmap_init(&ctx->import_cache, arena, 64);
    ctx->next_error_id = 0;

    context_init_errtable(ctx);

    ctx->settings.no_purge = false;
    ctx->settings.allocator_arc = false;
    ctx->settings.inject_enum_as_cstring = true;
    ctx->settings.default_cconv = NULL;
    ctx->settings.big_array_literal_as_flux_emit = -1;
    ctx->settings.resolve_method_call_as_call = true;
    ctx->cflags[0] = '\0';
    ctx->link_flags[0] = '\0';
}

const char* context_intern(CompilerContext *ctx, const char *str) {
    if (!ctx || !str) return NULL;
    return arena_strdup(ctx->arena, str);
}
