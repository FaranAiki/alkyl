#include "cli.h"
#include "../metalir/metalir.h"
#include "../alick/alick.h"
#include "../common/common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>

static SemanticCtx *global_sem_ctx = NULL;

static SemanticSettings default_sem_settings(void) {
    SemanticSettings s = {0};
    s.implicit_let = true;
    s.replace_variable = true;
    s.namespace_auto_search = true;
    return s;
}

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
                            sym = ns->inner_scope->symbols;
                        }
                        break;
                    }
                    ns = ns->next;
                }
                kw_idx = 999;
            }
        }
    }

    while (sym) {
        char *name = sym->name;
        sym = sym->next;

        if (!name) continue;

        if (word_len == 0 || strncmp(name, word, word_len) == 0) {
            return strdup(name);
        }
    }

    int num_keywords = sizeof(keywords) / sizeof(keywords[0]) - 1;
    while (kw_idx < num_keywords && keywords[kw_idx]) {
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
    rl_attempted_completion_over = 1;
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

    printf("\033[s");

    int len = rl_line_buffer ? (int)strlen(rl_line_buffer) : 0;
    if (len > rl_point) {
        printf("\033[%dC", len - rl_point);
    }
    printf("\033[K");

    if (rl_line_buffer && rl_point == len && rl_point > 0) {
        int word_len = 0;
        const char *word = get_last_word(rl_line_buffer, &word_len);

        if (word_len > 0) {
            const char *best_match = NULL;
            if (global_sem_ctx && global_sem_ctx->global_scope) {
                SemSymbol *sym = global_sem_ctx->global_scope->symbols;
                while (sym) {
                    if (sym->name && strncmp(sym->name, word, word_len) == 0) {
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

    printf("\033[u");
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
    int in_indent_block = 0;
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

        int line_len = strlen(line);
        char *trimmed = line;
        while(*trimmed == ' ' || *trimmed == '\t') trimmed++;

        if (!first_line && in_indent_block && *trimmed == '\0') {
            free(line);
            break;
        }

        if (first_line && strlen(line) > 0) add_history(line);
        if (total_len + line_len + 2 >= 4096) {
            printf("\033[31mInput too long!\033[0m\n");
            free(line); return NULL;
        }
        strcat(input_buffer, line);
        strcat(input_buffer, "\n");
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

        if (first_line && brace_depth == 0) {
            if (strncmp(trimmed, "if ", 3) == 0 || strncmp(trimmed, "if(", 3) == 0 ||
                strncmp(trimmed, "while ", 6) == 0 || strncmp(trimmed, "while(", 6) == 0 ||
                strncmp(trimmed, "for ", 4) == 0 || strncmp(trimmed, "for(", 4) == 0 ||
                strncmp(trimmed, "else", 4) == 0 ||
                strncmp(trimmed, "func ", 5) == 0 || strncmp(trimmed, "class ", 6) == 0 ||
                strncmp(trimmed, "struct ", 7) == 0 ||
                strncmp(trimmed, "flux ", 5) == 0) {
                if (line_len > 0 && trimmed[strlen(trimmed)-1] != ';' && trimmed[strlen(trimmed)-1] != '}') {
                    in_indent_block = 1;
                }
            }
        }

        free(line);
        if (brace_depth <= 0 && !in_indent_block) break;
        first_line = 0;
    }
    return input_buffer;
}

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
        rl_redisplay();
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

    SemanticSettings sem_settings = default_sem_settings();
    sem_settings.namespace_ausearch_warning = false;
    MetalirRunner *r = metalir_runner_create("ethyl_repl", &sem_settings, 0);
    global_sem_ctx = &r->sem;

    // This is the REPL's loaded module
    // make sure so that REPL's functionality is very good so
    // that users do not have to import many libraries
    metalir_load_module(r, "lib/std/print.aky");
    metalir_load_module(r, "lib/std/math.aky");

    rl_catch_signals = 0;
    signal(SIGINT, handle_sigint);
    rl_bind_key('\r', accept_line_clear_hint);
    rl_bind_key('\n', accept_line_clear_hint);

    int cmd_count = 0;

    while (1) {
        r->ctx.semantic_error_count = 0;
        r->ctx.error_count = 0;

        char *buffer = get_smart_input(&r->ast_arena, cmd_count);
        if (!buffer) break;

        int len = strlen(buffer);
        while(len > 0 && (buffer[len-1] == ' ' || buffer[len-1] == '\n' || buffer[len-1] == '\r')) len--;
        buffer[len] = '\0';

        if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "quit") == 0) {
            break;
        }

        ASTNode *root = metalir_parse(r, buffer, "REPL", NULL);
        if (!root || r->parser.has_error) continue;

        r->sem.current_source = buffer;
        r->sem.current_filename = "REPL";
        
        metalir_resolve_imports(r, &root);
        
        int sem_errs = sem_check_program(&r->sem, root);
        if (sem_errs > 0) {
            r->ctx.semantic_error_count = 0;
            r->ctx.error_count = 0;
            continue;
        }

        metalir_alir_generate(r, root);

        ASTNode *curr = root;
        int id = 0;
        while (curr) {
            if (curr->type == NODE_VAR_DECL) {
                metalir_run_var_decl(r, (VarDeclNode*)curr, id++);
            } else if (curr->type == NODE_CLASS) {
                metalir_run_class(r, curr, root);
            } else if (curr->type == NODE_FUNC_DEF) {
                metalir_run_func_def(r, curr);
            } else if (curr->type == NODE_LINK) {
                metalir_run_link(r, (LinkNode*)curr);
            } else if (curr->type == NODE_META || curr->type == NODE_POSTMETA) {
            } else if (curr->type != NODE_NAMESPACE && curr->type != NODE_ROOT &&
                       curr->type != NODE_ENUM && curr->type != NODE_ERRNUM) {
                metalir_run_expr(r, curr, id++, 1, NULL);
            }
            curr = curr->next;
        }

        cmd_count++;
    }
    metalir_runner_destroy(r);
    return 0;
}

int run_file(const char *filename) {
    char *code = read_file(filename);
    if (!code) {
        fprintf(stderr, "Could not read file: %s\n", filename);
        return 1;
    }

    SemanticSettings sem_settings = default_sem_settings();
    MetalirRunner *r = metalir_runner_create("ethyl_file", &sem_settings, 1);

    ASTNode *root = metalir_parse(r, code, filename, NULL);
    if (!root || r->parser.has_error) {
        free(code);
        metalir_runner_destroy(r);
        return 1;
    }

    r->sem.current_source = code;
    r->sem.current_filename = filename;
    
    fprintf(stderr, "[cli] calling metalir_resolve_imports, root=%p\n", (void*)root);
    metalir_resolve_imports(r, &root);
    fprintf(stderr, "[cli] after metalir_resolve_imports, root=%p\n", (void*)root);
    
    int sem_errs = sem_check_program(&r->sem, root);
    if (sem_errs > 0) {
        free(code);
        metalir_runner_destroy(r);
        return 1;
    }

    metalir_alir_generate(r, root);

    int alick_error = alick_check_module(r->module);
    if (alick_error > 0) {
        printf("Error occurred in alick.\n");
        free(code);
        metalir_runner_destroy(r);
        return 1;
    }

    ASTNode *curr = root;
    int id = 0;
    while (curr) {
        if (curr->type == NODE_VAR_DECL) {
            metalir_run_var_decl(r, (VarDeclNode*)curr, id++);
        } else if (curr->type == NODE_CLASS) {
            metalir_run_class(r, curr, root);
        } else if (curr->type == NODE_FUNC_DEF) {
            metalir_run_func_def(r, curr);
        } else if (curr->type == NODE_LINK) {
            metalir_run_link(r, (LinkNode*)curr);
        }
        curr = curr->next;
    }

    AlirFunction *main_fn = r->module->functions;
    while (main_fn) {
        if (strcmp(main_fn->name, "main") == 0) break;
        main_fn = main_fn->next;
    }

    int exit_code = 0;
    if (main_fn) {
        exit_code = (int)metalir_vm_execute(r->vm, r->module, main_fn, &r->sem, NULL, 0);
    } else {
        curr = root;
        while (curr) {
            if (curr->type == NODE_VAR_DECL) {
                metalir_run_var_decl(r, (VarDeclNode*)curr, id++);
            } else if (curr->type == NODE_CLASS) {
                metalir_run_class(r, curr, root);
            } else if (curr->type == NODE_FUNC_DEF) {
                metalir_run_func_def(r, curr);
            } else if (curr->type == NODE_LINK) {
                metalir_run_link(r, (LinkNode*)curr);
            } else if (curr->type == NODE_META || curr->type == NODE_POSTMETA) {
            } else if (curr->type != NODE_NAMESPACE && curr->type != NODE_ROOT &&
                       curr->type != NODE_ENUM && curr->type != NODE_ERRNUM) {
                VarType rt;
                long long res =                 metalir_run_expr(r, curr, id++, 0, &rt);
                if (rt.base != TYPE_VOID) {
                    printf("%lld\n", res);
                }
                exit_code = (int)res;
            }
            curr = curr->next;
        }
    }

    free(code);
    metalir_runner_destroy(r);
    return exit_code;
}

int import_module(const char *module_name) {
    printf("Importing module: %s\n", module_name);
    // here is metalir_load_module(r, module_name);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        return run_repl();
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--module") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -m/--module requires a module name\n");
                return 1;
            }
            return import_module(argv[i + 1]);
        }
    }

    char *filename = NULL;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            filename = argv[i];
            break;
        }
    }

    if (filename) {
        if (access(filename, F_OK) != -1) {
            return run_file(filename);
        } else {
            fprintf(stderr, "Error: File '%s' not found\n", filename);
            return 1;
        }
    }

    fprintf(stderr, "Usage: %s [file.aky] | -m <module> | --module <module>\n", argv[0]);
    return 1;
}
