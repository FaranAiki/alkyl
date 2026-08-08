#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "optlir/optlir.h"
#include "optlir/unused.h"
#include "optlir/local.h"
#include "common/linker.h"

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
        printf("Usage: %s <file.kyl|file.zyl> [-l<lib>] [--linker gcc|clang|lld|mold] | --lsp\n", argv[0]);
      return 1;
    }

    if (argc == 2 && streq_lit(argv[1], "--lsp")) {
        start_lsp_server();
        return 0;
    }

    char *filename = NULL;
    char link_flags[1024] = {0};
    char custom_output_basename[1024] = {0};
    int emit_alir = 0;
    int emit_balir = 0;
    int optimization_level = 0; // 0 = O0, 1 = O1, 2 = O2, 3 = O3, 4 = Os, 5 = Oz
    LinkerType current_linker = LINKER_GCC;

    ParserSettings parser_settings = {0};
    parser_set_default_import_paths(&parser_settings);
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
        } else if (streq_lit(argv[i], "--emit-alir")) {
            emit_alir = 1;
        } else if (streq_lit(argv[i], "--emit-balir")) {
            emit_balir = 1;
        } else if (streq_lit(argv[i], "--allow-vector-init")) {
            parser_settings.allow_vector_initialization = 1;
        } else if (streq_lit(argv[i], "-c")) {
            current_linker = LINKER_NONE;
        } else if (streq_lit(argv[i], "--unopt") || streq_lit(argv[i], "-O0")) {
            optimization_level = 0;
        } else if (streq_lit(argv[i], "--opt") || streq_lit(argv[i], "-O2")) {
            optimization_level = 2;
        } else if (streq_lit(argv[i], "-O1")) {
            optimization_level = 1;
        } else if (streq_lit(argv[i], "-O3")) {
            optimization_level = 3;
        } else if (streq_lit(argv[i], "-Os")) {
            optimization_level = 4;
        } else if (streq_lit(argv[i], "-Oz")) {
            optimization_level = 5;
        } else if (streq_lit(argv[i], "-o")) {
            if (i + 1 < argc) {
                strncpy(custom_output_basename, argv[++i], sizeof(custom_output_basename) - 1);
                custom_output_basename[sizeof(custom_output_basename) - 1] = '\0';
            }
        } else if (streq_lit(argv[i], "--linker")) {
            if (i + 1 < argc) {
                i++;
                if (streq_lit(argv[i], "gcc")) current_linker = LINKER_GCC;
                else if (streq_lit(argv[i], "clang")) current_linker = LINKER_CLANG;
                else if (streq_lit(argv[i], "lld")) current_linker = LINKER_LLD;
                else if (streq_lit(argv[i], "mold")) current_linker = LINKER_MOLD;
                else if (streq_lit(argv[i], "alynk")) current_linker = LINKER_MOLD;
            }
        } else {
            filename = argv[i];
        }
    }

    const char *output_basename_ptr = custom_output_basename[0] ? custom_output_basename : (optimization_level > 0 ? BASENAME_OPT : BASENAME);

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

#ifndef ALKYL_ENABLE_MLIR
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
#else
    AlirModule *alir_module = NULL;
    (void)alir_module;
    (void)emit_alir;
    (void)emit_balir;
    debug_step("Skipping ALIR generation since MLIR backend is enabled.");
#endif


    if (comp_ctx.error_count > 0) {
        fprintf(stderr, "Compilation aborted due to previous errors.\n");
        sem_cleanup(&sem_ctx);
        free(code);
        arena_free(&arena);
        return 1;
    }

#ifndef ALKYL_ENABLE_MLIR
    if (emit_balir) {
        alir_write_binary(alir_module, BASENAME ".balir");
    }

    debug_step("Finished alir check and analysis. Start alir optimization.");

    if (optimization_level > 0) {
        optlir_remove_unused(alir_module);

        optlir_optimize(alir_module, optimization_level);

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
            alir_emit_to_file(alir_module, BASENAME ".alir");
        }
    }
#endif

    debug_step("Finished alir optimization. Start code generation using " BACKEND_STRING " codegen");
    arena_reset(&arena);

    const char *active_output_basename = output_basename_ptr ? output_basename_ptr : (optimization_level > 0 ? BASENAME_OPT : BASENAME);
#ifndef ALKYL_ENABLE_MLIR
    int final_ret = backend_run_alir(alir_module, active_output_basename, link_flags, optimization_level, current_linker);
#else
    int final_ret = backend_run_semantic(&sem_ctx, root, active_output_basename, link_flags, optimization_level, current_linker);
#endif
    sem_cleanup(&sem_ctx);
    free(code);

    arena_free(&arena);

    return final_ret;
}
