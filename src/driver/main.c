#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "optlir/optlir.h"
#include "optlir/unused.h"
#include "optlir/local.h"

#define BASENAME "build/out"
#define BASENAME_OPT "build/opt_out"

#define str(s) #s
#define xstr(s) str(s)
#define BACKEND_STRING xstr(BACKEND)

#include "driver/lsp.h"

int main(int argc, char *argv[]) {
    Arena arena;
    CompilerContext comp_ctx;

    // TODO add this;
    if (argc < 2) {
      printf("Usage: %s <file.aky> [-l<lib>] | --lsp\n", argv[0]);
      return 1;
    }

    if (argc == 2 && strcmp(argv[1], "--lsp") == 0) {
        start_lsp_server();
        return 0;
    }

    char *filename = NULL;
    char link_flags[1024] = {0};
    char custom_output_basename[1024] = {0};
    int emit_alir = 0;
    int emit_balir = 0;
    int unopt_mode = 0;
    int opt_mode = 0;

    ParserSettings parser_settings = {0};
    // This is the default setting
    // for Alkyl as a compiled programming language
    parser_settings.namespace_auto_search = 1;
    parser_settings.namespace_ausearch_warning = 1;
    parser_settings.function_call_require_comma = 1;

    // We do not care about Windows initially
    // but if needed, use _mkdir
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
        } else if (strcmp(argv[i], "--unopt") == 0) {
            unopt_mode = 1;
        } else if (strcmp(argv[i], "--opt") == 0) {
            opt_mode = 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                strncpy(custom_output_basename, argv[++i], sizeof(custom_output_basename) - 1);
                custom_output_basename[sizeof(custom_output_basename) - 1] = '\0';
            }
        } else {
            filename = argv[i];
        }
    }

    const char *output_basename_ptr = custom_output_basename[0] ? custom_output_basename : ((opt_mode && !unopt_mode) ? BASENAME_OPT : BASENAME);

    if (!filename) {
        fprintf(stderr, "No input file specified\n");
        arena_free(&arena);
        return 1;
    }

    char *code = read_file(filename);
    if (!code) { fprintf(stderr, "Could not read file: %s\n", filename); return 1; }

    arena_init(&arena);
    context_init(&comp_ctx, &arena);

    Lexer l;
    lexer_init(&l, &comp_ctx, filename, code, NULL);

    debug_step("Finished lexing. Start parsing.");

    Parser p;
    parser_init(&p, &l, &parser_settings);

    ASTNode *root = parse_program(&p);

    // Resolve imports for AOT compiler
    resolve_imports(&p, &root);

    if (comp_ctx.error_count > 0) {
        free(code);
        arena_free(&arena);
        return 1;
    }

    SemanticCtx sem_ctx;
    sem_init(&sem_ctx, &comp_ctx, NULL);
    sem_ctx.current_source = code; // Enable source snippet printing for errors

    int sem_errors = sem_check_program(&sem_ctx, root);
    if (sem_errors > 0) {
        fprintf(stderr, "Semantic analysis failed with %d errors.\n", sem_errors);
        sem_cleanup(&sem_ctx);
        free(code);
        arena_free(&arena);
        return 1;
    }

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
        alir_emit_to_file(alir_module, BASENAME ".raw.alir");
    }

    debug_step("Finished alir. Start alir check and analysis.");

    int alick_error = alick_check_module(alir_module);
    if (alick_error > 0) {
      printf("Error occured in alick.\n");
      sem_cleanup(&sem_ctx);
      free(code);
      arena_free(&arena);
      return 1;
    }

    sem_cleanup(&sem_ctx);

    if (comp_ctx.error_count > 0) {
        fprintf(stderr, "Compilation aborted due to previous errors.\n");
        sem_cleanup(&sem_ctx);
        free(code);
        arena_free(&arena);
        return 1;
    }

    if (emit_balir) {
        alir_write_binary(alir_module, BASENAME ".balir");
    }

    debug_step("Finished alir check and analysis. Start alir optimization.");

    if (!unopt_mode) {
        optlir_remove_unused(alir_module);

        optlir_local_optimize(alir_module);

        // is this necessary tho?
        optlir_remove_unused(alir_module);

        int alick_error_post = alick_check_module(alir_module);
        if (alick_error_post > 0) {
          printf("Error occured in alick after optimization.\n");
          sem_cleanup(&sem_ctx);
          free(code);
          arena_free(&arena);
          return 1;
        }

        if (emit_alir) {
            alir_emit_to_file(alir_module, BASENAME ".opt.alir");
        }
    }

    debug_step("Finished alir optimization. Start code generation using " BACKEND_STRING " codegen");
    arena_reset(&arena);

    const char *active_output_basename = output_basename_ptr ? output_basename_ptr : (opt_mode ? BASENAME_OPT : BASENAME);
    int final_ret = backend_run(alir_module, active_output_basename, link_flags);
    free(code);

    arena_free(&arena);

    return final_ret;
}
