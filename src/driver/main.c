#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#define BASENAME "build/out"

#include "driver/lsp.h"

int main(int argc, char *argv[]) {
  Arena arena, arena_debug;
  CompilerContext comp_ctx, comp_ctx_debug;

  if (argc < 2) {
    printf("Usage: %s <file.aky> [-l<lib>] | --lsp\n", argv[0]);
    return 1;
  }

  if (argc == 2 && strcmp(argv[1], "--lsp") == 0) {
      start_lsp_server();
      return 0;
  }

  arena_init(&arena);
  arena_init(&arena_debug);
  context_init(&comp_ctx, &arena);
  context_init(&comp_ctx_debug, &arena_debug);

  char *filename = NULL;
  char link_flags[1024] = {0};
  int emit_alir = 0;
  int emit_balir = 0;
  
  ParserSettings parser_settings = {0};
  parser_settings.namespace_auto_search = 1;
  parser_settings.namespace_ausearch_warning = 1;
  parser_settings.function_call_require_comma = 1;

  mkdir("build", 0777);

  for (int i = 1; i < argc; i++) {
      if (strncmp(argv[i], "-l", 2) == 0) {
          if (strlen(link_flags) + strlen(argv[i]) + 2 < sizeof(link_flags)) {
              strcat(link_flags, " ");
              strcat(link_flags, argv[i]);
          } else {
              fprintf(stderr, "Too many link flags\n");
              return 1;
          }
      } else if (strcmp(argv[i], "--emit-alir") == 0) {
          emit_alir = 1;
      } else if (strcmp(argv[i], "--emit-balir") == 0) {
          emit_balir = 1;
      } else if (strcmp(argv[i], "--allow-vector-init") == 0) {
          parser_settings.allow_vector_initialization = 1;
      } else {
          filename = argv[i];
      }
  }

  if (!filename) {
      fprintf(stderr, "No input file specified\n");
      arena_free(&arena);
      arena_free(&arena_debug);
      return 1;
  }

  char *code = read_file(filename);
  if (!code) { fprintf(stderr, "Could not read file: %s\n", filename); return 1; }

  Lexer l;
  lexer_init(&l, &comp_ctx, filename, code, NULL);

  debug_step("Finished lexing. Start parsing.");

  // generate for debugging
  Lexer l_debug;
  lexer_init(&l_debug, &comp_ctx_debug, filename, code, NULL);

  to_token_out(&l_debug, BASENAME ".tok");

  Parser p;
  parser_init(&p, &l, &parser_settings);

  ASTNode *root = parse_program(&p);

  ASTNode *r = root;
  while (r) {
      if (r->type == NODE_COMPOUND) {
          ASTNode *cb = ((CompoundNode*)r)->body;
          while (cb) {
              cb = cb->next;
          }
      }
      r = r->next;
  }

  if (comp_ctx.error_count > 0) {
      free(code);
      arena_free(&arena);
      arena_free(&arena_debug);
      return 1;
  }

  // Parser p_debug;
  // parser_init(&p_debug, &l_debug, NULL);
  // ASTNode *root_debug = parse_program(&p_debug);

  to_ast_out(&p, root, BASENAME ".ast");

  debug_step("Start Semantic Analysis.");

  SemanticCtx sem_ctx;
  sem_init(&sem_ctx, &comp_ctx, NULL);
  sem_ctx.current_source = code; // Enable source snippet printing for errors

  int sem_errors = sem_check_program(&sem_ctx, root);
  if (sem_errors > 0) {
      fprintf(stderr, "Semantic analysis failed with %d errors.\n", sem_errors);
      sem_cleanup(&sem_ctx);
      free(code);
      return 1;
  }

  to_sem_out(&sem_ctx, BASENAME ".semc");

  // We keep sem_ctx alive if we want to use the Side Table for Codegen later.
  // For now, we clean it up as Codegen currently recalculates types (but safely now!)

  debug_step("Finished Semantic Analysis. Start macro-linking.");

  ASTNode *curr = root;
  while(curr) {
    if (curr->type == NODE_LINK) {
      LinkNode *lnk = (LinkNode*)curr;
      if (strlen(link_flags) + strlen(lnk->lib_name) + 4 < sizeof(link_flags)) {
        strcat(link_flags, " -l");
        strcat(link_flags, lnk->lib_name);
      }
    }
    curr = curr->next;
  }

  debug_step("Finished macro linking. Start generating Alkyl Intermediate Representation (alir).");

  // Pass to ALIR
  AlirModule *alir_module = alir_generate(&sem_ctx, root);
  if (emit_alir) {
      alir_emit_to_file(alir_module, BASENAME ".alir");
  }

  debug_step("Finished alir. Start alir check and analysis.");

  int alick_error = alick_check_module(alir_module);
  if (alick_error > 0) {
    printf("Error occured in alick.\n");
    exit(1);
  }

  sem_cleanup(&sem_ctx);

  if (comp_ctx.error_count > 0) {
      fprintf(stderr, "Compilation aborted due to previous errors.\n");
      return 1;
  }
  
  if (emit_balir) {
      alir_write_binary(alir_module, BASENAME ".balir");
  }

  debug_step("Finished alir check and analysis. Start Codegen using LLVM Codegen");
  arena_reset(&arena);

  int final_ret = backend_run(alir_module, BASENAME, link_flags);
  free(code);

  arena_free(&arena);
  arena_free(&arena_debug);

  return final_ret;
}
