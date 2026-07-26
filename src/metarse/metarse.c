#include "../metarse/metarse.h"
#include "../semantic/typestruct.h"

#include <string.h>
#include <dlfcn.h>

void executor_init(Executor *e, const char *module_name,
                   const SemanticSettings *sem_settings,
                   int function_call_require_comma) {
    arena_init(&e->ast_arena);
    context_init(&e->ctx, &e->ast_arena);

    Lexer dummy; LexerSettings ds = {0};
    ds.scope_style = SCOPE_INDENTATION;
    lexer_init(&dummy, &e->ctx, module_name, "", &ds);
    ParserSettings ps = {0};
    ps.function_call_require_comma = function_call_require_comma;
    ps.array_separator_with_space = 1;
    parser_init(&e->p, &dummy, &ps);

    arena_init(&e->vm_arena);
    e->vm = metalir_vm_init(&e->vm_arena);

    SemanticSettings ss = {0};
    if (sem_settings) memcpy(&ss, sem_settings, sizeof(SemanticSettings));
    sem_init(&e->sem, &e->ctx, &ss);
    e->module = alir_create_module(&e->ctx, module_name);
    e->sem.current_source = "";
    e->sem.current_filename = module_name;
    if (e->module) {
        e->module->src = "";
        e->module->filename = module_name;
    }
}

void executor_cleanup(Executor *e) {
    metalir_vm_free(e->vm);
    arena_free(&e->vm_arena);
    sem_cleanup(&e->sem);
    arena_free(&e->ast_arena);
}

ASTNode* executor_parse_source(Executor *e, const char *source,
                              const char *filename,
                              const LexerSettings *settings) {
    Lexer l;
    LexerSettings s = {0};
    if (settings) memcpy(&s, settings, sizeof(LexerSettings));
    lexer_init(&l, &e->ctx, filename, source, &s);
    e->p.l = &l;
    e->p.has_error = 0;
    e->p.token_pos = 0;
    e->p.tokens = NULL;
    e->p.token_capacity = 0;
    e->p.current_token.type = TOKEN_UNKNOWN;
    return parse_program(&e->p);
}

void executor_alir_generate(Executor *e, ASTNode *root) {
    AlirCtx a = {0};
    a.sem = &e->sem;
    a.module = e->module;
    alir_scan_and_register_classes(&a, root);
    alir_gen_functions_recursive(&a, root);
    if (e->module) {
        e->module->src = e->sem.current_source;
        e->module->filename = e->sem.current_filename;
    }
}

AlirFunction* alir_find_function(AlirModule *module, const char *name) {
    AlirFunction *f = module->functions;
    while (f) {
        if (strcmp(f->name, name) == 0) return f;
        f = f->next;
    }
    return NULL;
}

long long exec_var_decl(Executor *e, VarDeclNode *vd, int seq, const char *prefix) {
    long long initial_val = 0;

    if (vd->initializer) {
        char fname[64];
        snprintf(fname, sizeof(fname), "%s_init_%d", prefix, seq);

        FuncDefNode *fn = arena_alloc(&e->ast_arena, sizeof(FuncDefNode));
        memset(fn, 0, sizeof(FuncDefNode));
        fn->base.type = NODE_FUNC_DEF;
        fn->name = arena_strdup(&e->ast_arena, fname);
        fn->ret_type = sem_get_node_type(&e->sem, vd->initializer);
        fn->has_body = 1;

        ReturnNode *ret = arena_alloc(&e->ast_arena, sizeof(ReturnNode));
        memset(ret, 0, sizeof(ReturnNode));
        ret->base.type = NODE_RETURN;
        ret->value = vd->initializer;
        fn->body = (ASTNode*)ret;

        AlirCtx a = {0};
        a.sem = &e->sem;
        a.module = e->module;
        alir_gen_function_def(&a, fn, NULL);

        AlirFunction *cfn = alir_find_function(e->module, fname);
        if (cfn) initial_val = metalir_vm_execute(e->vm, e->module, cfn, &e->sem, NULL, 0);
    }

    VMGlobal *vg = arena_alloc(&e->vm_arena, sizeof(VMGlobal));
    vg->name = arena_strdup(&e->vm_arena, vd->name);

    VarType vt = vd->var_type;
    if (vt.base == TYPE_UNKNOWN && vd->initializer) vt = sem_get_node_type(&e->sem, vd->initializer);

    if (vt.array_size > 0) {
        if (initial_val) {
            vg->ptr_val = (void*)(intptr_t)initial_val;
        } else {
            vg->ptr_val = arena_alloc(&e->vm_arena, vt.array_size * 8);
        }
    } else {
        vg->ptr_val = arena_alloc(&e->vm_arena, 1024);
        *((long long*)vg->ptr_val) = initial_val;
    }
    vg->next = e->vm->globals;
    e->vm->globals = vg;

    return initial_val;
}

void exec_class(Executor *e, ASTNode *curr, ASTNode *root) {
    sem_check_node(&e->sem, curr);

    AlirCtx a = {0};
    a.sem = &e->sem;
    a.module = e->module;
    pass1_register(&a, curr);
    pass2_populate(&a, root, curr);
    alir_gen_functions_recursive(&a, curr);
}

void exec_func_def(Executor *e, ASTNode *curr) {
    FuncDefNode *f = (FuncDefNode*)curr;
    if (f->has_body && !f->is_macro) {
        AlirCtx a = {0};
        a.sem = &e->sem;
        a.module = e->module;
        alir_gen_function_def(&a, f, NULL);
    }
}

void exec_link(Executor *e, LinkNode *ln, int is_repl) {
    (void)e;
#ifndef _WIN32
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
    if (is_repl) {
        printf("\033[31mDynamic linking in REPL is not supported on Windows yet.\033[0m\n");
    } else {
        fprintf(stderr, "\033[33mWarning: Dynamic linking not supported in interpreted mode\033[0m\n");
    }
#endif
}

static void print_repl_value(VarType rt, long long result) {
    if (rt.base == TYPE_VOID) {
        printf("-> (void)\n");
    } else if (rt.base != TYPE_UNKNOWN) {
        if ((rt.base == TYPE_INT || rt.base == TYPE_LONG || rt.base == TYPE_LONG_LONG || rt.base == TYPE_UNSIGNED_INT || rt.base == TYPE_UNSIGNED_LONG || rt.base == TYPE_UNSIGNED_LONG_LONG) && rt.ptr_depth == 0 && rt.array_size == 0) {
            if (rt.is_unsigned || rt.base == TYPE_UNSIGNED_INT || rt.base == TYPE_UNSIGNED_LONG || rt.base == TYPE_UNSIGNED_LONG_LONG || rt.base == TYPE_UNSIGNED_CHAR)
                printf("-> %llu (%s)\n", (unsigned long long)result, sem_type_to_str(rt));
            else
                printf("-> %lld (%s)\n", result, sem_type_to_str(rt));
        }
        else if (rt.base == TYPE_SINGLE && rt.ptr_depth == 0 && rt.array_size == 0) {
            union { long long i; float f; } u; u.i = result;
            printf("-> %f (single)\n", (double)u.f);
        }
        else if (rt.base == TYPE_DOUBLE && rt.ptr_depth == 0 && rt.array_size == 0) {
            union { long long i; double d; } u; u.i = result;
            printf("-> %f (double)\n", u.d);
        }
        else if ((rt.base == TYPE_CLASS && rt.class_name && strcmp(rt.class_name, "string") == 0) || (rt.base == TYPE_CHAR && rt.ptr_depth == 1 && rt.array_size == 0))
            printf("-> %s (char*)\n", (char*)(intptr_t)result);
        else {
            if (rt.ptr_depth > 0 || rt.array_size > 0) {
                if (rt.array_size > 0 && result != 0) {
                    printf("-> [");
                    for (int i = 0; i < rt.array_size; i++) {
                        if (rt.array_depth > 0) {
                            long long inner_ptr = ((long long*)(intptr_t)result)[i];
                            printf("[");
                            for (int j = 0; j < rt.array_depth; j++) {
                                if (rt.base == TYPE_DOUBLE || rt.base == TYPE_SINGLE) {
                                    double val = ((double*)(intptr_t)inner_ptr)[j];
                                    printf("%f", val);
                                } else if (rt.base == TYPE_CHAR && rt.ptr_depth == 1) {
                                    long long val = ((long long*)(intptr_t)inner_ptr)[j];
                                    printf("\"%s\"", (char*)(intptr_t)val);
                                } else {
                                    long long val = ((long long*)(intptr_t)inner_ptr)[j];
                                    printf("%lld", val);
                                }
                                if (j < rt.array_depth - 1) printf(", ");
                            }
                            printf("]");
                        } else {
                            if (rt.base == TYPE_DOUBLE || rt.base == TYPE_SINGLE) {
                                double val = ((double*)(intptr_t)result)[i];
                                printf("%f", val);
                            } else if (rt.base == TYPE_CHAR && rt.ptr_depth == 1) {
                                long long val = ((long long*)(intptr_t)result)[i];
                                printf("\"%s\"", (char*)(intptr_t)val);
                            } else {
                                long long val = ((long long*)(intptr_t)result)[i];
                                printf("%lld", val);
                            }
                        }
                        if (i < rt.array_size - 1) printf(", ");
                    }
                    printf("] (%s)\n", sem_type_to_str(rt));
                } else {
                    printf("-> %p (%s)\n", (void*)(intptr_t)result, sem_type_to_str(rt));
                }
            }
            else
                printf("-> %lld (%s)\n", result, sem_type_to_str(rt));
        }
    }
}

long long exec_expr(Executor *e, ASTNode *curr, int seq, const char *prefix,
                    int print_rich, VarType *out_type) {
    char fname[64];
    snprintf(fname, sizeof(fname), "%s_expr_%d", prefix, seq);

    FuncDefNode *fn = arena_alloc(&e->ast_arena, sizeof(FuncDefNode));
    memset(fn, 0, sizeof(FuncDefNode));
    fn->base.type = NODE_FUNC_DEF;
    fn->name = arena_strdup(&e->ast_arena, fname);
    fn->ret_type = sem_get_node_type(&e->sem, curr);
    fn->has_body = 1;
    if (out_type) *out_type = fn->ret_type;

    if (curr->type == NODE_IF || curr->type == NODE_WHILE || curr->type == NODE_FOR_IN ||
        curr->type == NODE_LOOP || curr->type == NODE_SWITCH || curr->type == NODE_BREAK ||
        curr->type == NODE_CONTINUE || curr->type == NODE_RETURN || curr->type == NODE_DEFER) {
        fn->body = curr;
    } else {
        ReturnNode *ret = arena_alloc(&e->ast_arena, sizeof(ReturnNode));
        memset(ret, 0, sizeof(ReturnNode));
        ret->base.type = NODE_RETURN;
        ret->value = curr;
        fn->body = (ASTNode*)ret;
    }

    AlirCtx a = {0};
    a.sem = &e->sem;
    a.module = e->module;
    alir_gen_function_def(&a, fn, NULL);

    AlirFunction *cfn = alir_find_function(e->module, fname);
    if (!cfn) return 0;

    if (print_rich) {
        alir_emit_to_file(e->module, "repl_debug.alir");
    }

    long long result = metalir_vm_execute(e->vm, e->module, cfn, &e->sem, NULL, 0);

    if (curr->type == NODE_ASSIGN && ((AssignNode*)curr)->is_implicit_let) {
        if (fn->ret_type.array_size > 0) {
            VMGlobal *g = e->vm->globals;
            while (g) {
                if (strcmp(g->name, ((AssignNode*)curr)->name) == 0) {
                    g->ptr_val = (void*)(intptr_t)result;
                    break;
                }
                g = g->next;
            }
        }
    }

    if (print_rich) {
        print_repl_value(fn->ret_type, result);
    }

    return result;
}
