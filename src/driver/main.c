/**
 * @file main.c
 * @brief Main driver entry point.
 */
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
#include "common/debug.h"
#include "parser/c_parser.h"
#include "parser/link.h"
#include "parser/emitter.h"

#define BASENAME "build/out"
#define BASENAME_OPT "build/opt_out"

#define str(s) #s
#define xstr(s) str(s)
#define BACKEND_STRING xstr(BACKEND)

#include "driver/lsp.h"

/**
 * @brief Recursively prints a C AST node and its children for debugging.
 * @param node The AST node to print.
 * @param indent Current indentation level.
 */
static void print_c_ast_node(ASTNode *node, int indent) {
    if (!node) return;
    char indent_str[64] = {0};
    for (int i = 0; i < indent && i < 30; i++) strcat(indent_str, "  ");
    
    switch (node->type) {
        case NODE_FUNC_DEF: {
            FuncDefNode *fn = (FuncDefNode*)node;
            debug_c_header("%sFUNC_DEF: %s extern=%d has_body=%d cconv=%s\n",
                    indent_str, fn->name, fn->is_extern, fn->has_body, fn->cconv ? fn->cconv : "none");
            break;
        }
        case NODE_STRUCT: {
            StructNode *sn = (StructNode*)node;
            debug_c_header("%sSTRUCT: %s is_union=%d has_body=%d\n",
                    indent_str, sn->name, sn->is_union, sn->has_body);
            ASTNode *member = sn->members;
            while (member) {
                print_c_ast_node(member, indent + 1);
                member = member->next;
            }
            break;
        }
        case NODE_ENUM: {
            EnumNode *en = (EnumNode*)node;
            debug_c_header("%sENUM: %s\n", indent_str, en->name);
            EnumEntry *entry = en->entries;
            while (entry) {
                debug_c_header("%s  %s = %d\n", indent_str, entry->name, entry->value);
                entry = entry->next;
            }
            break;
        }
        case NODE_VAR_DECL: {
            VarDeclNode *var = (VarDeclNode*)node;
            debug_c_header("%sVAR_DECL: %s\n", indent_str, var->name);
            break;
        }
        default:
            debug_c_header("%sNODE_TYPE=%d\n", indent_str, node->type);
            break;
    }
}

/**
 * @brief Compiler entry point for the Alkyl compiler.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit code.
 */
int main(int argc, char **argv) {
    char *filename = NULL;
    char *c_header_file = NULL;
    int parse_c_mode = 0;
    int emit_alir = 0;
    int emit_balir = 0;
    int emit_ast = 0;
    int optimization_level = 0;
    char link_flags[1024] = {0};
    char custom_output_basename[256] = {0};
    LinkerType current_linker = LINKER_GCC;
    ParserSettings parser_settings = {0};

    // We do not care about Windows initially
    // but if needed, use _mkdir
    mkdir("build", 0777);

    if (argc < 2) {
        printf("Usage: %s <file.kyl|file.zyl> [-l<lib>] [--linker gcc|clang|lld|mold] | --lsp | --parse-c <file.h>\n", argv[0]);
      return __LINE__;
    }

    if (argc == 2 && streq_lit(argv[1], "--lsp")) {
        start_lsp_server();
        return 0;
    }

    parser_set_default_import_paths(&parser_settings);
    parser_settings.namespace_auto_search = 1;
    parser_settings.namespace_ausearch_warning = 1;
    parser_settings.function_call_require_comma = 1;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-l", 2) == 0) {
            if (strlen(link_flags) + strlen(argv[i]) + 2 < sizeof(link_flags)) {
                strcat(link_flags, " ");
                strcat(link_flags, argv[i]);
            } else {
                fprintf(stderr, "Too many link flags\n");
                return __LINE__;
            }
        } else if (streq_lit(argv[i], "--emit-alir")) {
            emit_alir = 1;
        } else if (streq_lit(argv[i], "--emit-balir")) {
            emit_balir = 1;
        } else if (streq_lit(argv[i], "--emit-ast")) {
            emit_ast = 1;
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
                i++;
                strncpy(custom_output_basename, argv[i], sizeof(custom_output_basename) - 1);
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
        } else if (streq_lit(argv[i], "--parse-c") || streq_lit(argv[i], "--c-header")) {
            if (i + 1 < argc) {
                i++;
                c_header_file = argv[i];
                parse_c_mode = 1;
                emit_ast = 1;
            } else {
                fprintf(stderr, "--parse-c requires a file argument\n");
                return __LINE__;
            }
        } else {
            filename = argv[i];
        }
    }

    Arena arena;
    CompilerContext comp_ctx;

    if (parse_c_mode) {
        if (!c_header_file) {
            fprintf(stderr, "No C header file specified for --parse-c\n");
            return __LINE__;
        }

        arena_init(&arena);
        context_init(&comp_ctx, &arena);

        char *code = c_preprocess_header(&comp_ctx, c_header_file);
        if (!code) {
            fprintf(stderr, "Could not read or preprocess C header file: %s\n", c_header_file);
            return __LINE__;
        }

        CParser cp;
        c_parser_init(&cp, &comp_ctx, c_header_file, code);

        debug_c_header("Parsing C header: %s\n", c_header_file);
        ASTNode *root = c_parse_header(&cp);

        if (emit_ast) {
            Parser p = {0};
            p.ctx = &comp_ctx;
            char *ast_str = parser_to_string(&p, root);
            if (ast_str) {
                fprintf(stdout, "%s", ast_str);
            }
        } else {
            int node_count = 0;
            ASTNode *curr = root;
            while (curr) {
                node_count++;
                print_c_ast_node(curr, 0);
                curr = curr->next;
            }

            debug_c_header("Total nodes: %d\n", node_count);
            debug_c_header("Typedefs registered: %d\n", cp.typedef_map.size);
            debug_c_header("Macros defined: %d\n", cp.defines.count);
        }

        // free(code); // Allocated on arena, freed with arena_free
        arena_free(&arena);
        return 0;
    }

    const char *output_basename_ptr = custom_output_basename[0] ? custom_output_basename : (optimization_level > 0 ? BASENAME_OPT : BASENAME);

    if (!filename) {
        fprintf(stderr, "No input file specified\n");
        return __LINE__;
    }

    char *code = read_file(filename);
    if (!code) { fprintf(stderr, "Could not read file: %s\n", filename); return __LINE__; }

    arena_init(&arena);
    context_init(&comp_ctx, &arena);

    Lexer l;
    lexer_init(&l, &comp_ctx, filename, code, NULL);

    debug_step("Finished lexing. Start parsing.");

    Parser p;
    parser_init(&p, &l, &parser_settings);

    ASTNode *root = parse_program(&p);

    ASTNode *lnk_curr = root;
    while (lnk_curr) {
        if (lnk_curr->type == NODE_LINK) {
            LinkNode *lnk = (LinkNode*)lnk_curr;
            add_pkg_config_flags(&comp_ctx, lnk->lib_name);
        }
        lnk_curr = lnk_curr->next;
    }

    // Resolve imports for AOT compiler
    resolve_imports(&p, &root);

    if (emit_ast) {
        char *ast_str = parser_to_string(&p, root);
        if (ast_str) {
            fprintf(stdout, "%s", ast_str);
        }
        free(code);
        arena_free(&arena);
        return 0;
    }

    if (comp_ctx.error_count > 0) {
        free(code);
        arena_free(&arena);
        return __LINE__;
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
        return __LINE__;
    }

    // We keep sem_ctx alive if we want to use the Side Table for Codegen later.
    // For now, we clean it up as Codegen currently recalculates types (but safely now!)

    debug_step("Finished Semantic Analysis. Start macro-linking.");

    if (comp_ctx.link_flags[0]) {
        if (strlen(link_flags) + strlen(comp_ctx.link_flags) + 1 < sizeof(link_flags)) {
            strcat(link_flags, comp_ctx.link_flags);
        }
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
      return __LINE__;
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
        return __LINE__;
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
          return __LINE__;
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
