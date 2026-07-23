#ifndef CONTEXT_H
#define CONTEXT_H

#include "hashmap.h"
#include "arena.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct {
  bool no_purge;
  bool allocator_arc;
  char *default_cconv;
  bool inject_enum_as_cstring;
  bool double_quote_as_string;
} CompilerSettings;

// Holds the global state for a single compilation session
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
} CompilerContext;

// Initialize the context with a provided arena
void context_init(CompilerContext *ctx, Arena *arena);
const char* context_intern(CompilerContext *ctx, const char *str);

#endif // CONTEXT_H
