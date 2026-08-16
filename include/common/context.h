#ifndef CONTEXT_H
#define CONTEXT_H

#include "hashmap.h"
#include "arena.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Compiler settings that control compilation behavior.
 */
typedef struct {
  bool no_purge;
  bool allocator_arc;
  char *default_cconv;
  bool inject_enum_as_cstring;
  bool double_quote_as_string;
  long long big_array_literal_as_flux_emit;
  bool resolve_method_call_as_call;
} CompilerSettings;

// Holds the global state for a single compilation session
/**
 * @brief Holds the global state for a single compilation session.
 */
typedef struct {
  Arena *arena;

  int lexer_error_count;
  int parser_error_count;
  int semantic_error_count;
  int alir_error_count;
  int error_count;

  // Diagnostic State (formerly globals in diagnostic.c)
  char current_namespace[256];
  char last_reported_namespace[256];
  char last_reported_filename[1024];

  HashMap string_pool;
  HashMap error_table;
  int next_error_id;
  void *macro_head;
  CompilerSettings settings;
  HashMap import_cache;
  char cflags[4096];
  char link_flags[4096];
} CompilerContext;

/**
 * @brief Initializes the context with a provided arena.
 * @param ctx The compiler context to initialize.
 * @param arena The arena allocator for this context.
 */
void context_init(CompilerContext *ctx, Arena *arena);
/**
 * @brief Interns a string into the compiler context's arena.
 * @param ctx The compiler context.
 * @param str The null-terminated string to intern.
 * @return A pointer to the interned string, or NULL on failure.
 */
const char* context_intern(CompilerContext *ctx, const char *str);

#endif // CONTEXT_H
