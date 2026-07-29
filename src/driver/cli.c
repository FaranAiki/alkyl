#include "cli.h"
#include "../metalir/metalir.h"
#include "../alick/alick.h"
#include "../common/common.h"
#include "keyboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static SemanticSettings default_sem_settings(void) {
    SemanticSettings s = {0};
    s.implicit_let = true;
    s.replace_variable = true;
    s.namespace_auto_search = true;
    return s;
}

int run_repl(void) {
    printf("\033[36mEthyl (Alkyl interpreter) version 0.0.1 \033[0m\n");
    printf("Type \033[33m'exit'\033[0m or \033[33m'quit'\033[0m to leave.\n\n");

    SemanticSettings sem_settings = default_sem_settings();
    sem_settings.namespace_ausearch_warning = false;
    MetalirRunner *r = metalir_runner_create("ethyl_repl", &sem_settings, 0);

    metalir_load_module(r, "lib/std/print.aky");
    metalir_load_module(r, "lib/std/math.aky");

    signal(SIGINT, SIG_IGN);

    int cmd_count = 0;

    while (1) {
        r->ctx.semantic_error_count = 0;
        r->ctx.error_count = 0;

        char *buffer = get_smart_input(&r->ast_arena, cmd_count, &r->sem);
        if (!buffer) break;

        int len = strlen(buffer);
        while(len > 0 && (buffer[len-1] == ' ' || buffer[len-1] == '\n' || buffer[len-1] == '\r')) len--;
        buffer[len] = '\0';

        if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "quit") == 0) {
            break;
        }

        ASTNode *root = metalir_parse(r, buffer, "ethyl_repl", NULL);
        if (!root || r->parser.has_error) continue;

        r->sem.current_source = buffer;
        r->sem.current_filename = "ethyl_repl";

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

    metalir_resolve_imports(r, &root);

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
