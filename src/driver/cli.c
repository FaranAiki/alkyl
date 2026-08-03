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

static void print_symbol_info(SemanticCtx *ctx, VarType rt) {
    SemSymbol *sym = sem_symbol_lookup_type(ctx, rt.class_name);
    if (sym && sym->inner_scope) {
        printf("\033[36m%s\033[0m %s {\n", rt.base == TYPE_NAMESPACE ? "namespace" : "class", rt.class_name);
        SemSymbol *mem = sym->inner_scope->symbols;
        while(mem) {
            if (mem->name) {
                if (mem->kind == SYM_FUNC) {
                    char *ret_str = sem_type_to_str(mem->type);
                    printf("    \033[90mfunc\033[0m %s %s(", ret_str, mem->name);
                    Parameter *p = mem->params;
                    while (p) {
                        char *p_str = sem_type_to_str(p->type);
                        printf("%s", p_str);
                        if (p->next) printf(", ");
                        p = p->next;
                    }
                    if (mem->is_variadic) {
                        if (mem->params) printf(", ");
                        printf("...");
                    }
                    printf(")\n");
                } else if (mem->kind == SYM_VAR) {
                    char *type_str = sem_type_to_str(mem->type);
                    printf("    \033[90mvar\033[0m %s %s\n", type_str, mem->name);
                } else {
                    const char *kind_str = "unknown";
                    if (mem->kind == SYM_CLASS) kind_str = "class";
                    else if (mem->kind == SYM_ENUM) kind_str = "enum";
                    else if (mem->kind == SYM_NAMESPACE) kind_str = "namespace";
                    printf("    \033[90m%s\033[0m %s\n", kind_str, mem->name);
                }
            }
            mem = mem->next;
        }
        printf("}\n");
    } else {
        printf("%s %s\n", rt.base == TYPE_NAMESPACE ? "namespace" : "class", rt.class_name);
    }
}

static void display_init(void) {
    printf("\033[36mEthyl (Alkyl interpreter) by Faran Aiki \033[0m\n");
    printf("Type \033[33m'exit'\033[0m or \033[33m'quit'\033[0m to leave.\n\n");
}

int run_repl(void) {
    display_init();

    SemanticSettings sem_settings = default_sem_settings();
    sem_settings.namespace_ausearch_warning = false;
    MetalirRunner *r = metalir_runner_create("ethyl_repl", &sem_settings, 0);

    metalir_load_module(r, "std/ethyl");

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

        if (streq(buffer, "exit") || streq(buffer, "quit")) {
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
                long long val = metalir_run_var_decl(r, (VarDeclNode*)curr, id++);
                VarDeclNode *vd = (VarDeclNode*)curr;
                VarType vt = vd->var_type;
                if (vt.base == TYPE_UNKNOWN && vd->initializer) vt = sem_get_node_type(&r->sem, vd->initializer);
                metalir_print_repl_value(vt, val);

                if (vt.base != TYPE_VOID && vt.base != TYPE_UNKNOWN) {
                    SemSymbol *res_sym = sem_symbol_lookup(&r->sem, "res", NULL);
                    if (!res_sym) {
                        res_sym = sem_symbol_add(&r->sem, "res", SYM_VAR, vt);
                        res_sym->is_mutable = true;
                        res_sym->is_initialized = true;
                    } else {
                        res_sym->type = vt;
                    }
                    VMGlobal *g = r->vm->globals;
                    void *ptr = NULL;
                    while(g) { if (streq(g->name, "res")) { ptr = g->ptr_val; break; } g = g->next; }
                    if (!ptr) {
                        VMGlobal *vg = arena_alloc(&r->vm_arena, sizeof(VMGlobal));
                        vg->name = arena_strdup(&r->vm_arena, "res");
                        vg->ptr_val = arena_alloc(&r->vm_arena, 1024);
                        vg->next = r->vm->globals;
                        r->vm->globals = vg;
                        ptr = vg->ptr_val;
                    }
                    if (vt.base == TYPE_CLASS && vt.ptr_depth == 0 && val) {
                        int struct_size = 1024;
                        if (r->module && vt.class_name) { struct_size = alir_get_struct_size(r->module, vt.class_name); if (struct_size < 8) struct_size = 8; }
                        memcpy(ptr, (void*)(intptr_t)val, struct_size);
                    } else if (vt.base == TYPE_CLASS && vt.ptr_depth == 0) {
                        /* val is 0 (no initializer), ptr_val already zero-allocated */
                    } else { *((long long*)ptr) = val; }
                }
            } else if (curr->type == NODE_CLASS) {
                metalir_run_class(r, curr, root);
            } else if (curr->type == NODE_FUNC_DEF) {
                if (!((FuncDefNode*)curr)->is_macro) {
                    metalir_run_func_def(r, curr);
                }
            } else if (curr->type == NODE_LINK) {
                metalir_run_link(r, (LinkNode*)curr);
            } else if (curr->type == NODE_META || curr->type == NODE_POSTMETA) {
} else if (curr->type != NODE_NAMESPACE && curr->type != NODE_ROOT &&
                   curr->type != NODE_ENUM && curr->type != NODE_ERRNUM &&
                   curr->type != NODE_IMPORT) {
                VarType chk = sem_get_node_type(&r->sem, curr);
                int is_type_sym = 0;
                if ((chk.base == TYPE_NAMESPACE || chk.base == TYPE_CLASS) && curr->type == NODE_VAR_REF) {
                    SemSymbol *sym = sem_symbol_lookup(&r->sem, ((VarRefNode*)curr)->name, NULL);
                    if (sym && (sym->kind == SYM_CLASS || sym->kind == SYM_NAMESPACE)) {
                        is_type_sym = 1;
                    }
                }
                if (is_type_sym) {
                    print_symbol_info(&r->sem, chk);
                } else {
                    VarType expr_rt;
                    long long res_val = metalir_run_expr(r, curr, id++, 1, &expr_rt);

                    if (expr_rt.base != TYPE_VOID && expr_rt.base != TYPE_UNKNOWN) {
                        SemSymbol *res_sym = sem_symbol_lookup(&r->sem, "res", NULL);
                        if (!res_sym) {
                            res_sym = sem_symbol_add(&r->sem, "res", SYM_VAR, expr_rt);
                            res_sym->is_mutable = true;
                            res_sym->is_initialized = true;
                        } else {
                            res_sym->type = expr_rt;
                        }
                        VMGlobal *g = r->vm->globals;
                        void *ptr = NULL;
                        while(g) { if (streq(g->name, "res")) { ptr = g->ptr_val; break; } g = g->next; }
                        if (!ptr) {
                            VMGlobal *vg = arena_alloc(&r->vm_arena, sizeof(VMGlobal));
                            vg->name = arena_strdup(&r->vm_arena, "res");
                            vg->ptr_val = arena_alloc(&r->vm_arena, 1024);
                            vg->next = r->vm->globals;
                            r->vm->globals = vg;
                            ptr = vg->ptr_val;
                        }
                        if (expr_rt.base == TYPE_CLASS && expr_rt.ptr_depth == 0 && res_val) {
                            int struct_size = 1024;
                            if (r->module && expr_rt.class_name) { struct_size = alir_get_struct_size(r->module, expr_rt.class_name); if (struct_size < 8) struct_size = 8; }
                            memcpy(ptr, (void*)(intptr_t)res_val, struct_size);
                        } else if (expr_rt.base == TYPE_CLASS && expr_rt.ptr_depth == 0) {
                            /* res_val is 0 (no initializer), ptr_val already zero-allocated */
                        } else { *((long long*)ptr) = res_val; }

                        /* Update target VM global for regular assignments (e.g. v = ...) */
                        if (curr->type == NODE_ASSIGN && !((AssignNode*)curr)->is_implicit_let) {
                            const char *target_name = ((AssignNode*)curr)->name;
                            VMGlobal *vg2 = r->vm->globals;
                            while(vg2) {
                                if (streq(vg2->name, target_name)) {
                                    if (expr_rt.base == TYPE_CLASS && expr_rt.ptr_depth == 0 && res_val) {
                                        int struct_size = 1024;
                                        if (r->module && expr_rt.class_name) { struct_size = alir_get_struct_size(r->module, expr_rt.class_name); if (struct_size < 8) struct_size = 8; }
                                        memcpy(vg2->ptr_val, (void*)(intptr_t)res_val, struct_size);
                                    } else if (expr_rt.base == TYPE_CLASS && expr_rt.ptr_depth == 0) {
                                        /* res_val is 0, ptr_val already zero-allocated */
                                    } else {
                                        *((long long*)vg2->ptr_val) = res_val;
                                    }
                                    break;
                                }
                                vg2 = vg2->next;
                            }
                        }
                    }
                }
            }
            curr = curr->next;
        }

        cmd_count++;
#ifdef DEBUG_ANY
        alir_emit_to_file(r->module, "repl_debug_after.alir");
#endif // DEBUG_ANY
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
            if (!((FuncDefNode*)curr)->is_macro) {
                metalir_run_func_def(r, curr);
            }
        } else if (curr->type == NODE_LINK) {
            metalir_run_link(r, (LinkNode*)curr);
        }
        curr = curr->next;
    }

    AlirFunction *main_fn = r->module->functions;
    while (main_fn) {
        if (streq(main_fn->name, "main")) break;
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
                if (!((FuncDefNode*)curr)->is_macro) {
                    metalir_run_func_def(r, curr);
                }
            } else if (curr->type == NODE_LINK) {
                metalir_run_link(r, (LinkNode*)curr);
            } else if (curr->type == NODE_META || curr->type == NODE_POSTMETA) {
            } else if (curr->type != NODE_NAMESPACE && curr->type != NODE_ROOT &&
                       curr->type != NODE_ENUM && curr->type != NODE_ERRNUM &&
                       curr->type != NODE_IMPORT) {
                VarType chk = sem_get_node_type(&r->sem, curr);
                int is_type_sym = 0;
                if ((chk.base == TYPE_NAMESPACE || chk.base == TYPE_CLASS) && curr->type == NODE_VAR_REF) {
                    SemSymbol *sym = sem_symbol_lookup(&r->sem, ((VarRefNode*)curr)->name, NULL);
                    if (sym && (sym->kind == SYM_CLASS || sym->kind == SYM_NAMESPACE)) {
                        is_type_sym = 1;
                    }
                }
                if (is_type_sym) {
                    print_symbol_info(&r->sem, chk);
                } else {
                    VarType rt;
                    long long res = metalir_run_expr(r, curr, id++, 0, &rt);
                    if (rt.base != TYPE_VOID) {
                        printf("%lld\n", res);
                    }
                    exit_code = (int)res;
                }
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
        if (streq(argv[i], "-m") || streq(argv[i], "--module")) {
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

    fprintf(stderr, "Usage: %s [file.kyl|file.zyl] | -m <module> | --module <module>\n", argv[0]);
    return 1;
}
