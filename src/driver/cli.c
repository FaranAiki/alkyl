#include "cli.h"
#include "../common/arena.h"
#include "../common/context.h"
#include "../parser/parser.h"
#include "../semantic/semantic.h"
#include "../alir/alir.h"
#include <meta/vm.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "codegen_llvm/codegen.h"
#include <readline/readline.h>
#include <readline/history.h>
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include "../common/diagnostic.h"

// Global context for autocompletion
static SemanticCtx *global_sem_ctx = NULL;

#include <ctype.h>

static const char* get_last_word(const char* line, int* word_len) {
    int len = line ? strlen(line) : 0;
    if (len == 0) {
        *word_len = 0;
        return line ? line : "";
    }
    int i = len - 1;
    while (i >= 0 && (isalnum((unsigned char)line[i]) || line[i] == '_')) i--;
    i++;
    *word_len = len - i;
    return line + i;
}

static char* ethyl_generator(const char* text, int state) {
    (void)text;
    static SemSymbol *sym = NULL;
    static int word_len = 0;
    static const char *word = NULL;
    static const char *keywords[] = {
        "let", "mut", "if", "else", "while", "for", "in", "return", "switch", "case", 
        "break", "continue", "func", "class", "struct", "union", "enum", "errnum", 
        "import", "namespace", "true", "false", "null", "void", "extern", "pure", "pristine", NULL
    };
    static int kw_idx = 0;

    if (!state) {
        if (!global_sem_ctx || !global_sem_ctx->global_scope) return NULL;
        sym = global_sem_ctx->global_scope->symbols;
        kw_idx = 0;

        word = get_last_word(rl_line_buffer, &word_len);
        
        int word_start_idx = word - rl_line_buffer;
        if (word_start_idx > 0 && rl_line_buffer[word_start_idx - 1] == '.') {
            int i = word_start_idx - 2;
            while (i >= 0 && (isalnum((unsigned char)rl_line_buffer[i]) || rl_line_buffer[i] == '_')) i--;
            i++;
            int ns_len = (word_start_idx - 1) - i;
            if (ns_len > 0) {
                char ns_name[256];
                snprintf(ns_name, ns_len + 1, "%s", rl_line_buffer + i);
                
                sym = NULL;
                SemSymbol *ns = global_sem_ctx->global_scope->symbols;
                while(ns) {
                    if (strcmp(ns->name, ns_name) == 0) {
                        if (ns->kind == SYM_NAMESPACE && ns->inner_scope) {
                            sym = ns->inner_scope->symbols;
                        } else if (ns->type.base == TYPE_CLASS && ns->type.class_name) {
                            SemSymbol *cls = global_sem_ctx->global_scope->symbols;
                            while(cls) {
                                if (strcmp(cls->name, ns->type.class_name) == 0 && cls->kind == SYM_CLASS && cls->inner_scope) {
                                    sym = cls->inner_scope->symbols;
                                    break;
                                }
                                cls = cls->next;
                            }
                        } else if (ns->kind == SYM_CLASS && ns->inner_scope) {
                            // Static class members
                            sym = ns->inner_scope->symbols;
                        }
                        break;
                    }
                    ns = ns->next;
                }
                kw_idx = 999; // Don't autocomplete keywords after a dot
            }
        }
    }

    while (sym) {
        char *name = sym->name;
        sym = sym->next; // advance for next call

        // Only use prefix match for autocomplete
        if (word_len == 0 || strncmp(name, word, word_len) == 0) {
            return strdup(name);
        }
    }

    while (keywords[kw_idx]) {
        const char *name = keywords[kw_idx++];
        if (word_len == 0 || strncmp(name, word, word_len) == 0) {
            return strdup(name);
        }
    }

    return NULL;
}

static char** ethyl_completion(const char* text, int start, int end) {
    (void)start;
    (void)end;
    rl_attempted_completion_over = 1; // Don't fall back to filename completion
    return rl_completion_matches(text, ethyl_generator);
}

static void display_matches_hook(char **matches, int num_matches, int max_length) {
    printf("\033[90m\n");
    rl_display_match_list(matches, num_matches, max_length);
    printf("\033[0m");
    rl_forced_update_display();
}

static void ethyl_redisplay(void) {
    rl_redisplay();

    printf("\033[s"); // Save cursor
    
    int len = rl_line_buffer ? (int)strlen(rl_line_buffer) : 0;
    if (len > rl_point) {
        printf("\033[%dC", len - rl_point); // Move to end of line
    }
    printf("\033[K"); // Clear trailing ghost text

    if (rl_line_buffer && rl_point == len && rl_point > 0) {
        int word_len = 0;
        const char *word = get_last_word(rl_line_buffer, &word_len);

        if (word_len > 0) {
            const char *best_match = NULL;
            if (global_sem_ctx && global_sem_ctx->global_scope) {
                SemSymbol *sym = global_sem_ctx->global_scope->symbols;
                while (sym) {
                    if (strncmp(sym->name, word, word_len) == 0) {
                        best_match = sym->name;
                        break;
                    }
                    sym = sym->next;
                }
            }
            if (!best_match) {
                static const char *keywords[] = {
                    "let", "mut", "if", "else", "while", "for", "in", "return", "switch", "case", 
                    "break", "continue", "func", "class", "struct", "union", "enum", "errnum", 
                    "import", "namespace", "true", "false", "null", "void", "extern", "pure", "pristine", NULL
                };
                for (int i = 0; keywords[i]; i++) {
                    if (strncmp(keywords[i], word, word_len) == 0) {
                        best_match = keywords[i];
                        break;
                    }
                }
            }

            if (best_match && (int)strlen(best_match) > word_len) {
                const char *hint = best_match + word_len;
                printf("\033[90m%s\033[0m", hint);
            }
        }
    }

    printf("\033[u"); // Restore cursor
    fflush(stdout);
}

char* get_smart_input(Arena* arena, int cmd_count) {
    char prompt[128];
    sprintf(prompt, "\033[32mIn [%d]:\033[0m ", cmd_count);

    char *input_buffer = arena_alloc(arena, 4096);
    if (!input_buffer) return NULL;
    input_buffer[0] = '\0';
    int total_len = 0;
    int brace_depth = 0;
    char *line;
    int first_line = 1;
    while (1) {
        if (!first_line) {
            char indent_prompt[128] = "... ";
            for(int i=0; i<brace_depth && i<10; i++) strcat(indent_prompt, "    ");
            line = readline(indent_prompt);
        } else {
            line = readline(prompt);
        }
        if (!line) return NULL;
        if (first_line && strlen(line) > 0) add_history(line);
        int line_len = strlen(line);
        if (total_len + line_len + 2 >= 4096) {
            printf("\033[31mInput too long!\033[0m\n");
            free(line); return NULL;
        }
        strcat(input_buffer, line);
        strcat(input_buffer, " ");
        total_len += line_len + 1;
        int in_string = 0;
        int in_char = 0;
        for (int i = 0; i < line_len; i++) {
            if (line[i] == '"' && !in_char) in_string = !in_string;
            if (line[i] == '\'' && !in_string) in_char = !in_char;
            if (!in_string && !in_char) {
                if (line[i] == '{') brace_depth++;
                if (line[i] == '}') brace_depth--;
            }
        }
        free(line);
        if (brace_depth <= 0) break;
        first_line = 0;
    }
    return input_buffer;
}

#include <signal.h>

extern int rl_newline(int count, int key);
static int accept_line_clear_hint(int count, int key) {
    if (rl_line_buffer) {
        rl_point = rl_end;
        rl_redisplay();
        printf("\033[K");
        fflush(stdout);
    }
    return rl_newline(count, key);
}

static void handle_sigint(int sig) {
    (void)sig;
    if (rl_line_buffer) {
        rl_point = rl_end;
        rl_redisplay(); // This will trigger ethyl_redisplay which clears the ghost text
    }
    printf("^C\n");
    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
}

int run_repl(void) {
    printf("\033[36mEthyl (Alkyl interpreter) version 0.0.1 \033[0m\n");
    printf("Type \033[33m'exit'\033[0m or \033[33m'quit'\033[0m to leave.\n\n");

    rl_attempted_completion_function = ethyl_completion;
    rl_completion_display_matches_hook = display_matches_hook;
    rl_redisplay_function = ethyl_redisplay;

    Arena ast_arena;
    arena_init(&ast_arena);

    CompilerContext ctx;
    context_init(&ctx, &ast_arena);

    // Initialize global parser state (we will share the types_map across REPL lines)
    Lexer dummy_l;
    LexerSettings dummy_settings = {0};
    dummy_settings.import_require_double_quotes = 0;
    lexer_init(&dummy_l, &ctx, "REPL", "", &dummy_settings);
    Parser p;
    parser_init(&p, &dummy_l, NULL);

    SemanticSettings sem_settings = {0};
    sem_settings.implicit_let = true;
    sem_settings.replace_variable = true;
    sem_settings.namespace_auto_search = true;
    sem_settings.namespace_ausearch_warning = false;
    SemanticCtx sem;
    sem_init(&sem, &ctx, &sem_settings);
    global_sem_ctx = &sem;

    // We keep one AlirModule appending stuff
    AlirModule *module = alir_create_module(&ctx, "ethyl_repl");

    Arena vm_arena;
    arena_init(&vm_arena);
    MetaVM *vm = meta_vm_init(&vm_arena);

    // Setup readline autocomplete
    rl_catch_signals = 0;
    signal(SIGINT, handle_sigint);
    rl_attempted_completion_function = ethyl_completion;
    rl_completion_display_matches_hook = display_matches_hook;
    rl_redisplay_function = ethyl_redisplay;
    rl_bind_key('\r', accept_line_clear_hint);
    rl_bind_key('\n', accept_line_clear_hint);

    int cmd_count = 0;

    while (1) {
        char *buffer = get_smart_input(&ast_arena, cmd_count);
        if (!buffer) break;

        if (strcmp(buffer, "exit ") == 0 || strcmp(buffer, "quit ") == 0) {
            break;
        }

        int len = strlen(buffer);
        while(len > 0 && buffer[len-1] == ' ') len--;
        buffer[len] = '\0';

        Lexer l;
        LexerSettings settings = {0};
        settings.require_semicolons = 0;
        settings.import_require_double_quotes = 0;
        lexer_init(&l, &ctx, "REPL", buffer, &settings);

        p.l = &l;
        p.has_error = 0;
        p.token_pos = 0;
        p.tokens = NULL;
        p.token_capacity = 0;
        p.current_token.type = TOKEN_UNKNOWN;

        ASTNode *root = parse_program(&p);
        if (!root || p.has_error) continue;

        sem.current_source = buffer;
        sem.current_filename = "REPL";

        ASTNode **tail = &root;
        while (*tail && (*tail)->next) tail = &(*tail)->next;
        if (*tail) sem.ast_tail = &(*tail)->next;
        else sem.ast_tail = tail;

        sem_scan_top_level(&sem, root);
        ASTNode *curr = root;
        while (curr) {
            if (curr->type == NODE_VAR_DECL) {
                sem_check_var_decl(&sem, (VarDeclNode*)curr, 0);
            } else {
                sem_check_node(&sem, curr);
            }
            curr = curr->next;
        }

        if (ctx.semantic_error_count > 0) {
            ctx.semantic_error_count = 0; // reset for next line
            continue;
        }

        // First pass: ALIR compile all declarations (functions, classes) in the current line
        curr = root;
        while(curr) {
            if (curr->type == NODE_FUNC_DEF) {
                if (((FuncDefNode*)curr)->has_body && !((FuncDefNode*)curr)->is_macro) {
                    AlirCtx alir_ctx;
                    memset(&alir_ctx, 0, sizeof(AlirCtx));
                    alir_ctx.sem = &sem;
                    alir_ctx.module = module;
                    alir_gen_function_def(&alir_ctx, (FuncDefNode*)curr, NULL);
                }
            } else if (curr->type == NODE_CLASS) {
                AlirCtx alir_ctx;
                memset(&alir_ctx, 0, sizeof(AlirCtx));
                alir_ctx.sem = &sem;
                alir_ctx.module = module;
                pass1_register(&alir_ctx, curr); pass2_populate(&alir_ctx, root, curr);
            }
            curr = curr->next;
        }

        // Evaluate top level expressions using MetaVM
        curr = root;
        while(curr) {
            if (curr->type == NODE_VAR_DECL) {
                VarDeclNode *vd = (VarDeclNode*)curr;
                long long initial_val = 0;

                if (vd->initializer) {
                    char func_name[64];
                    sprintf(func_name, "__repl_init_%d", cmd_count);

                    FuncDefNode *fn = arena_alloc(&ast_arena, sizeof(FuncDefNode));
                    fn->base.type = NODE_FUNC_DEF;
                    fn->name = func_name;
                    fn->ret_type = sem_get_node_type(&sem, vd->initializer);
                    fn->has_body = 1;

                    ReturnNode *ret = arena_alloc(&ast_arena, sizeof(ReturnNode));
                    ret->base.type = NODE_RETURN;
                    ret->value = vd->initializer;
                    fn->body = (ASTNode*)ret;

                    printf("DEBUG CLI: curr->type=%d\n", curr->type); ASTNode **old_tail = sem.ast_tail; ASTNode *first_new = NULL; if (old_tail) first_new = *old_tail; sem_check_func_def(&sem, fn); printf("DEBUG CLI: &sem.ast_tail=%p, old_tail=%p, sem.ast_tail=%p, first_new=%p, *old_tail=%p\n", &sem.ast_tail, old_tail, sem.ast_tail, first_new, old_tail ? *old_tail : NULL); if (old_tail && *old_tail != first_new) { ASTNode *n = *old_tail; while(n) { if (n->type == NODE_FUNC_DEF && !((FuncDefNode*)n)->is_macro) { AlirCtx actx; memset(&actx, 0, sizeof(AlirCtx)); actx.sem = &sem; actx.module = module; alir_gen_function_def(&actx, (FuncDefNode*)n, NULL); printf("DEBUG CLI: compiled function %s (mangled: %s)\n", ((FuncDefNode*)n)->name, ((FuncDefNode*)n)->mangled_name); } n = n->next; } }

                    AlirCtx alir_ctx;
                    memset(&alir_ctx, 0, sizeof(AlirCtx));
                    alir_ctx.sem = &sem;
                    alir_ctx.module = module;

                    alir_gen_function_def(&alir_ctx, fn, NULL);

                    AlirFunction *compiled_fn = module->functions;
                    while (compiled_fn) {
                        if (strcmp(compiled_fn->name, func_name) == 0) break;
                        compiled_fn = compiled_fn->next;
                    }

                    if (compiled_fn) {
                        initial_val = meta_vm_execute(vm, module, compiled_fn, &sem, NULL, 0);
                    }
                }

                // Add to MetaVM global memory map
                VMGlobal *vg = arena_alloc(&vm_arena, sizeof(VMGlobal));
                vg->name = arena_strdup(&vm_arena, vd->name);
                
                VarType vt = vd->var_type;
                if (vt.base == TYPE_UNKNOWN && vd->initializer) vt = sem_get_node_type(&sem, vd->initializer);
                
                if (vt.array_size > 0) {
                    if (initial_val) {
                        vg->ptr_val = (void*)(intptr_t)initial_val;
                    } else {
                        vg->ptr_val = arena_alloc(&vm_arena, vt.array_size * 8);
                    }
                } else {
                    vg->ptr_val = arena_alloc(&vm_arena, 1024);
                    *((long long*)vg->ptr_val) = initial_val;
                }
                vg->next = vm->globals;
                vm->globals = vg;

            } else if (curr->type == NODE_CLASS) {
                // Class definitions go straight to the current sem context
                sem_check_node(&sem, curr);

                // Register in ALIR module
                AlirCtx alir_ctx;
                memset(&alir_ctx, 0, sizeof(AlirCtx));
                alir_ctx.sem = &sem;
                alir_ctx.module = module;

                pass1_register(&alir_ctx, curr);
                pass2_populate(&alir_ctx, root, curr);

            } else if (curr->type == NODE_FUNC_DEF) {
                if (((FuncDefNode*)curr)->has_body && !((FuncDefNode*)curr)->is_macro) {
                    AlirCtx alir_ctx;
                    memset(&alir_ctx, 0, sizeof(AlirCtx));
                    alir_ctx.sem = &sem;
                    alir_ctx.module = module;
                    alir_gen_function_def(&alir_ctx, (FuncDefNode*)curr, NULL);
                }
            } else if (curr->type == NODE_LINK) {
// TODO make sure so that WIN32 knows things, but this is significantly difficult: don't care lmao
// TODO self-hosting means alkyl must have proper define: e.g. if defined(os.linux) or if os.name == "Linux" in meta
#ifndef _WIN32
                LinkNode *ln = (LinkNode*)curr;
                char libname[256];
#ifdef __APPLE__
                snprintf(libname, sizeof(libname), "lib%s.dylib", ln->lib_name);
#else
                snprintf(libname, sizeof(libname), "lib%s.so", ln->lib_name);
#endif
                void *handle = dlopen(libname, RTLD_GLOBAL | RTLD_NOW);
                if (!handle) {
                    snprintf(libname, sizeof(libname), "%s", ln->lib_name);
                    handle = dlopen(libname, RTLD_GLOBAL | RTLD_NOW);
                }
#ifndef __APPLE__
                if (!handle) {
                    snprintf(libname, sizeof(libname), "lib%s.so.6", ln->lib_name);
                    handle = dlopen(libname, RTLD_GLOBAL | RTLD_NOW);
                }
#endif
                if (!handle) {
                    printf("\033[31mFailed to link '%s': %s\033[0m\n", ln->lib_name, dlerror());
                } else {
                    printf("\033[32mLinked '%s' successfully.\033[0m\n", ln->lib_name);
                }
#else
                printf("\033[31mDynamic linking in REPL is not supported on Windows yet.\033[0m\n");
#endif
            } else if (curr->type != NODE_CLASS && curr->type != NODE_NAMESPACE && curr->type != NODE_ROOT && curr->type != NODE_LINK) {
                char func_name[64];
                static int repl_expr_count = 0; sprintf(func_name, "__repl_expr_%d", ++repl_expr_count);

                // Wrap in a synthetic function
                FuncDefNode *fn = arena_alloc(&ast_arena, sizeof(FuncDefNode));
                fn->base.type = NODE_FUNC_DEF;
                fn->name = func_name;

                VarType ret_type = sem_get_node_type(&sem, curr);
                fn->ret_type = ret_type;
                fn->has_body = 1;

                if (curr->type == NODE_IF || curr->type == NODE_WHILE || curr->type == NODE_FOR_IN ||
                    curr->type == NODE_LOOP || curr->type == NODE_SWITCH || curr->type == NODE_BREAK ||
                    curr->type == NODE_CONTINUE || curr->type == NODE_RETURN || curr->type == NODE_DEFER) {
                    fn->body = curr;
                } else {
                    ReturnNode *ret = arena_alloc(&ast_arena, sizeof(ReturnNode));
                    ret->base.type = NODE_RETURN;
                    ret->value = curr;
                    fn->body = (ASTNode*)ret;
                }

                printf("DEBUG CLI: curr->type=%d\n", curr->type); ASTNode **old_tail = sem.ast_tail; ASTNode *first_new = NULL; if (old_tail) first_new = *old_tail; sem_check_func_def(&sem, fn); printf("DEBUG CLI: &sem.ast_tail=%p, old_tail=%p, sem.ast_tail=%p, first_new=%p, *old_tail=%p\n", &sem.ast_tail, old_tail, sem.ast_tail, first_new, old_tail ? *old_tail : NULL); if (old_tail && *old_tail != first_new) { ASTNode *n = *old_tail; while(n) { if (n->type == NODE_FUNC_DEF && !((FuncDefNode*)n)->is_macro) { AlirCtx actx; memset(&actx, 0, sizeof(AlirCtx)); actx.sem = &sem; actx.module = module; alir_gen_function_def(&actx, (FuncDefNode*)n, NULL); printf("DEBUG CLI: compiled function %s (mangled: %s)\n", ((FuncDefNode*)n)->name, ((FuncDefNode*)n)->mangled_name); } n = n->next; } }

                AlirCtx alir_ctx;
                memset(&alir_ctx, 0, sizeof(AlirCtx));
                alir_ctx.sem = &sem;
                alir_ctx.module = module;

                // Hack: generate the function
                alir_gen_function_def(&alir_ctx, fn, NULL);

                AlirFunction *compiled_fn = module->functions;
                while (compiled_fn) {
                    if (strcmp(compiled_fn->name, func_name) == 0) break;
                    compiled_fn = compiled_fn->next;
                }

                // TODO make this more MECE & orthgonal
                if (compiled_fn) {
                    alir_emit_to_file(module, "repl_debug.alir");
                    long long exit_code = meta_vm_execute(vm, module, compiled_fn, &sem, NULL, 0);
                    
                    if (curr->type == NODE_ASSIGN && ((AssignNode*)curr)->is_implicit_let) {
                        if (ret_type.array_size > 0) {
                            VMGlobal *g = vm->globals;
                            while (g) {
                                if (strcmp(g->name, ((AssignNode*)curr)->name) == 0) {
                                    g->ptr_val = (void*)(intptr_t)exit_code;
                                    break;
                                }
                                g = g->next;
                            }
                        }
                    }

                    if (ret_type.base != TYPE_VOID && ret_type.base != TYPE_UNKNOWN) {
                        if ((ret_type.base == TYPE_INT || ret_type.base == TYPE_LONG) && ret_type.ptr_depth == 0 && ret_type.array_size == 0) {
                            if (ret_type.is_unsigned) printf("-> %llu (unsigned int)\n", (unsigned long long)exit_code);
                            else printf("-> %lld (int)\n", exit_code);
                        }
                        else if (ret_type.base == TYPE_SINGLE && ret_type.ptr_depth == 0 && ret_type.array_size == 0) {
                            union { long long i; float f; } u; u.i = exit_code;
                            printf("-> %f (single)\n", (double)u.f);
                        }
                        else if (ret_type.base == TYPE_DOUBLE && ret_type.ptr_depth == 0 && ret_type.array_size == 0) {
                            union { long long i; double d; } u; u.i = exit_code;
                            printf("-> %f (double)\n", u.d);
                        }
                        else if ((ret_type.base == TYPE_CLASS && ret_type.class_name && strcmp(ret_type.class_name, "string") == 0) || (ret_type.base == TYPE_CHAR && ret_type.ptr_depth == 1 && ret_type.array_size == 0))
                            printf("-> %s (char*)\n", (char*)(intptr_t)exit_code);
                        else {
                            if (ret_type.ptr_depth > 0 || ret_type.array_size > 0) {
                                if (ret_type.array_size > 0 && exit_code != 0) {
                                    printf("-> [");
                                    for (int i = 0; i < ret_type.array_size; i++) {
                                        if (ret_type.base == TYPE_DOUBLE || ret_type.base == TYPE_SINGLE) {
                                            double val = ((double*)(intptr_t)exit_code)[i];
                                            printf("%f", val);
                                        } else {
                                            long long val = ((long long*)(intptr_t)exit_code)[i];
                                            printf("%lld", val);
                                        }
                                        if (i < ret_type.array_size - 1) printf(", ");
                                    }
                                    printf("] (%s)\n", sem_type_to_str(ret_type));
                                } else {
                                    printf("-> %p (%s)\n", (void*)(intptr_t)exit_code, sem_type_to_str(ret_type));
                                }
                            }
                            else
                                printf("-> %lld (%s)\n", exit_code, sem_type_to_str(ret_type));
                        }
                    }
                }
            }
            curr = curr->next;
        }

        cmd_count++;
    }
    meta_vm_free(vm);
    sem_cleanup(&sem);
    arena_free(&ast_arena);
    return 0;
}

int main() {
  return run_repl();
}
