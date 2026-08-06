#include "metalir.h"
#include "../metarse/metarse.h"
#include "../common/common.h"
#include <dlfcn.h>

MetalirRunner* metalir_runner_create(const char *module_name,
                                      const SemanticSettings *sem_settings,
                                      int function_call_require_comma) {
    MetalirRunner *r = arena_alloc_type(&(Arena){0}, MetalirRunner);
    if (!r) return NULL;

    arena_init(&r->ast_arena);
    context_init(&r->ctx, &r->ast_arena);

    LexerSettings ds = {0};
    ds.scope_style = SCOPE_INDENTATION;
    lexer_init(&r->lexer, &r->ctx, module_name, "", &ds);
    ParserSettings ps = {0};
    ps.function_call_require_comma = function_call_require_comma;
    ps.array_separator_with_space = 1;
    parser_set_default_import_paths(&ps);
    if (module_name && strstr(module_name, "repl")) {
        ps.multiplication_if_digit_word = 1;
        ps.exponentation_if_word_digit = 1;
        ps.greedy_space_calls = 0;
    }
    r->parser.l = &r->lexer;
    parser_init(&r->parser, &r->lexer, &ps);

    arena_init(&r->vm_arena);
    r->vm = metalir_vm_init(&r->vm_arena);

    SemanticSettings ss = {0};
    if (sem_settings) memcpy(&ss, sem_settings, sizeof(SemanticSettings));
    sem_init(&r->sem, &r->ctx, &ss);
    r->module = alir_create_module(&r->ctx, module_name);
    r->sem.current_source = "";
    r->sem.current_filename = module_name;
    r->module->src = "";
    r->module->filename = module_name;

    return r;
}

void metalir_runner_destroy(MetalirRunner *r) {
    if (!r) return;
    metalir_vm_free(r->vm);
    arena_free(&r->vm_arena);
    sem_cleanup(&r->sem);
    arena_free(&r->ast_arena);
}

ASTNode* metalir_parse(MetalirRunner *r, const char *source,
                       const char *filename, const LexerSettings *settings) {
    LexerSettings s = {0};
    if (settings) memcpy(&s, settings, sizeof(LexerSettings));
    lexer_init(&r->lexer, &r->ctx, filename, source, &s);
    r->parser.l = &r->lexer;
    r->parser.has_error = 0;
    r->parser.token_pos = 0;
    r->parser.tokens = NULL;
    r->parser.token_capacity = 0;
    r->parser.current_token.type = TOKEN_UNKNOWN;
    return parse_program(&r->parser);
}

void metalir_sem_check(MetalirRunner *r, ASTNode *root) {
    r->sem.current_source = r->parser.l->src;
    r->sem.current_filename = r->parser.l->filename;

    ASTNode **tail = &root;
    while (*tail && (*tail)->next) tail = &(*tail)->next;
    if (*tail) r->sem.ast_tail = &(*tail)->next;
    else r->sem.ast_tail = tail;

    sem_scan_top_level(&r->sem, root);
    ASTNode *curr = root;
    while (curr) {
        if (curr->type == NODE_VAR_DECL) {
            sem_check_var_decl(&r->sem, (VarDeclNode*)curr, 0);
        } else {
            sem_check_node(&r->sem, curr);
        }
        curr = curr->next;
    }
}

void metalir_alir_generate(MetalirRunner *r, ASTNode *root) {
    AlirCtx a = {0};
    a.sem = &r->sem;
    a.module = r->module;
    alir_scan_and_register_classes(&a, root);
    scan_and_fold_consts(&a, root);
    alir_gen_functions_recursive(&a, root, NULL);
    r->module->src = r->sem.current_source;
    r->module->filename = r->sem.current_filename;
}

long long metalir_execute_alir(MetalirRunner *r, const char *func_name) {
    AlirFunction *f = r->module->functions;
    while (f) {
        if (streq(f->name, func_name)) break;
        f = f->next;
    }
    if (!f) return 0;
    return metalir_vm_execute(r->vm, r->module, f, &r->sem, NULL, 0);
}

static AlirFunction* find_func(MetalirRunner *r, const char *name) {
    AlirFunction *f = r->module->functions;
    while (f) {
        if (streq(f->name, name)) return f;
        f = f->next;
    }
    return NULL;
}

void metalir_print_repl_value(VarType rt, long long result) {
    if (rt.is_func_ptr) {
        printf("-> %p (%s)\n", (void*)(intptr_t)result, sem_type_to_str(rt));
    } else if (rt.base == TYPE_VOID && rt.ptr_depth == 0) {
        printf("-> (void)\n");
    } else if (rt.base == TYPE_UNKNOWN && rt.ptr_depth == 0) {
        printf("-> (unknown)\n");
    } else if (rt.base == TYPE_BOOL && rt.ptr_depth == 0 && rt.array_size == 0) {
        printf("-> %s (bool)\n", result ? "true" : "false");
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
        else if ((rt.base == TYPE_CLASS && rt.class_name && streq(rt.class_name, "string")) || (rt.base == TYPE_CHAR && rt.ptr_depth == 1 && rt.array_size == 0)) {
            if ((unsigned long long)result < 0x10000) {
                printf("-> %p (char*)\n", (void*)(intptr_t)result);
            } else {
                printf("-> %s (char*)\n", (char*)(intptr_t)result);
            }
        }
        else if (rt.base == TYPE_CLASS && rt.ptr_depth == 0 && rt.array_size == 0)
            printf("-> %p (%s)\n", (void*)(intptr_t)result, sem_type_to_str(rt));
        else {
            if (rt.ptr_depth > 0 || rt.array_size > 0) {
                if (rt.array_size > 0 && result != 0) {
                    printf("-> [");
                    for (int i = 0; i < rt.array_size; i++) {
                        if (rt.array_depth > 0) {
                            long long inner_ptr = ((long long*)(intptr_t)result)[i];
                            printf("[");
                            for (int j = 0; j < rt.array_depth; j++) {
                                if (rt.base == TYPE_DOUBLE) {
                                    double val = ((double*)(intptr_t)inner_ptr)[j];
                                    printf("%f", val);
                                } else if (rt.base == TYPE_SINGLE) {
                                    float val = ((float*)(intptr_t)inner_ptr)[j];
                                    printf("%f", (double)val);
                                } else if (rt.base == TYPE_CHAR && rt.ptr_depth == 1) {
                                    long long val = ((long long*)(intptr_t)inner_ptr)[j];
                                    printf("\"%s\"", (char*)(intptr_t)val);
                                } else {
                                    long long val = 0;
                                    if (rt.base == TYPE_CHAR || rt.base == TYPE_UNSIGNED_CHAR || rt.base == TYPE_BOOL)
                                        val = ((char*)(intptr_t)inner_ptr)[j];
                                    else if (rt.base == TYPE_SHORT)
                                        val = ((short*)(intptr_t)inner_ptr)[j];
                                    else if (rt.base == TYPE_INT || rt.base == TYPE_UNSIGNED_INT || rt.base == TYPE_ENUM)
                                        val = ((int*)(intptr_t)inner_ptr)[j];
                                    else
                                        val = ((long long*)(intptr_t)inner_ptr)[j];
                                    printf("%lld", val);
                                }
                                if (j < rt.array_depth - 1) printf(", ");
                            }
                            printf("]");
                        } else {
                            if (rt.base == TYPE_DOUBLE) {
                                double val = ((double*)(intptr_t)result)[i];
                                printf("%f", val);
                            } else if (rt.base == TYPE_SINGLE) {
                                float val = ((float*)(intptr_t)result)[i];
                                printf("%f", (double)val);
                            } else if (rt.base == TYPE_CHAR && rt.ptr_depth == 1) {
                                long long val = ((long long*)(intptr_t)result)[i];
                                printf("\"%s\"", (char*)(intptr_t)val);
                            } else {
                                long long val = 0;
                                if (rt.base == TYPE_CHAR || rt.base == TYPE_UNSIGNED_CHAR || rt.base == TYPE_BOOL)
                                    val = ((char*)(intptr_t)result)[i];
                                else if (rt.base == TYPE_SHORT)
                                    val = ((short*)(intptr_t)result)[i];
                                else if (rt.base == TYPE_INT || rt.base == TYPE_UNSIGNED_INT || rt.base == TYPE_ENUM)
                                    val = ((int*)(intptr_t)result)[i];
                                else
                                    val = ((long long*)(intptr_t)result)[i];
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
            else {
                if (rt.base == TYPE_CHAR) {
                    char c = (char)result;
                    if (c >= 32 && c <= 126) printf("-> %lld | '%c' (%s)\n", result, c, sem_type_to_str(rt));
                    else if (c == '\n') printf("-> %lld | '\\n' (%s)\n", result, sem_type_to_str(rt));
                    else if (c == '\r') printf("-> %lld | '\\r' (%s)\n", result, sem_type_to_str(rt));
                    else if (c == '\t') printf("-> %lld | '\\t' (%s)\n", result, sem_type_to_str(rt));
                    else if (c == '\0') printf("-> %lld | '\\0' (%s)\n", result, sem_type_to_str(rt));
                    else printf("-> %lld (%s)\n", result, sem_type_to_str(rt));
                } else {
                    printf("-> %lld (%s)\n", result, sem_type_to_str(rt));
                }
            }
        }
    }
}

long long metalir_run_var_decl(MetalirRunner *r, VarDeclNode *vd, int seq) {
    long long initial_val = 0;

    if (vd->initializer) {
        char fname[64];
        snprintf(fname, sizeof(fname), "__init_%d", seq);

        FuncDefNode *fn = arena_alloc(&r->ast_arena, sizeof(FuncDefNode));
        memset(fn, 0, sizeof(FuncDefNode));
        fn->base.type = NODE_FUNC_DEF;
        fn->name = arena_strdup(&r->ast_arena, fname);
        fn->ret_type = sem_get_node_type(&r->sem, vd->initializer);
        fn->has_body = 1;

        ReturnNode *ret = arena_alloc(&r->ast_arena, sizeof(ReturnNode));
        memset(ret, 0, sizeof(ReturnNode));
        ret->base.type = NODE_RETURN;
        ret->value = vd->initializer;
        fn->body = (ASTNode*)ret;

        AlirCtx a = {0};
        a.sem = &r->sem;
        a.module = r->module;
        alir_gen_function_def(&a, fn, NULL);

        AlirFunction *cfn = find_func(r, fname);
        if (cfn) initial_val = metalir_vm_execute(r->vm, r->module, cfn, &r->sem, NULL, 0);

        VarType init_type = sem_get_node_type(&r->sem, vd->initializer);
        if (vd->var_type.base == TYPE_DOUBLE && init_type.base != TYPE_DOUBLE) {
            double d = (double)initial_val;
            memcpy(&initial_val, &d, sizeof(d));
        } else if (vd->var_type.base == TYPE_SINGLE && init_type.base != TYPE_SINGLE) {
            float f = (float)initial_val;
            memcpy(&initial_val, &f, sizeof(f));
        }
    }

    VMGlobal *vg = arena_alloc(&r->vm_arena, sizeof(VMGlobal));
    vg->name = arena_strdup(&r->vm_arena, vd->name);

    VarType vt = vd->var_type;
    if (vt.base == TYPE_UNKNOWN && vd->initializer) vt = sem_get_node_type(&r->sem, vd->initializer);

    if (vt.array_size > 0) {
        if (initial_val) {
            vg->ptr_val = (void*)(intptr_t)initial_val;
        } else {
            vg->ptr_val = arena_alloc(&r->vm_arena, vt.array_size * 8);
        }
    } else if (vt.base == TYPE_CLASS && vt.ptr_depth == 0) {
        vg->ptr_val = arena_alloc(&r->vm_arena, 1024);
        if (initial_val) {
            int struct_size = 1024;
            if (r->module && vt.class_name) {
                struct_size = alir_get_struct_size(r->module, vt.class_name);
                if (struct_size < 8) struct_size = 8;
            }
            memcpy(vg->ptr_val, (void*)(intptr_t)initial_val, struct_size);
        }
    } else {
        vg->ptr_val = arena_alloc(&r->vm_arena, 1024);
        *((long long*)vg->ptr_val) = initial_val;
    }
    vg->next = r->vm->globals;
    r->vm->globals = vg;

    return initial_val;
}

void metalir_run_class(MetalirRunner *r, ASTNode *curr, ASTNode *root) {
    sem_check_node(&r->sem, curr);

    AlirCtx a = {0};
    a.sem = &r->sem;
    a.module = r->module;
    hashmap_init(&a.class_map, a.module->compiler_ctx ? a.module->compiler_ctx->arena : NULL, 64);
    pass1_register(&a, curr, NULL);
    pass2_populate(&a, root, curr, NULL);
    alir_gen_functions_recursive(&a, curr, NULL);
}

void metalir_run_func_def(MetalirRunner *r, ASTNode *curr) {
    FuncDefNode *f = (FuncDefNode*)curr;
    if (f->has_body && !f->is_macro) {
        AlirCtx a = {0};
        a.sem = &r->sem;
        a.module = r->module;
        alir_gen_function_def(&a, f, NULL);
    }
}

void metalir_run_link(MetalirRunner *r, LinkNode *ln) {
    (void)r;
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
    fprintf(stderr, "\033[33mWarning: Dynamic linking not supported\033[0m\n");
#endif
}

long long metalir_run_expr(MetalirRunner *r, ASTNode *curr, int seq,
                           int print_rich, VarType *out_type) {
    char fname[64];
    snprintf(fname, sizeof(fname), "__expr_%d", seq);

    FuncDefNode *fn = arena_alloc(&r->ast_arena, sizeof(FuncDefNode));
    memset(fn, 0, sizeof(FuncDefNode));
    fn->base.type = NODE_FUNC_DEF;
    fn->name = arena_strdup(&r->ast_arena, fname);
    fn->ret_type = sem_get_node_type(&r->sem, curr);
    fn->has_body = 1;
    if (out_type) *out_type = fn->ret_type;

    if (curr->type == NODE_IF || curr->type == NODE_WHILE || curr->type == NODE_FOR_IN ||
        curr->type == NODE_LOOP || curr->type == NODE_SWITCH || curr->type == NODE_BREAK ||
        curr->type == NODE_CONTINUE || curr->type == NODE_RETURN || curr->type == NODE_DEFER) {
        fn->body = curr;
    } else {
        ReturnNode *ret = arena_alloc(&r->ast_arena, sizeof(ReturnNode));
        memset(ret, 0, sizeof(ReturnNode));
        ret->base.type = NODE_RETURN;
        ret->value = curr;
        fn->body = (ASTNode*)ret;
    }

    AlirCtx a = {0};
    a.sem = &r->sem;
    a.module = r->module;
    alir_gen_function_def(&a, fn, NULL);

    AlirFunction *cfn = find_func(r, fname);
    if (!cfn) return 0;

    if (print_rich) {
        alir_emit_to_file(r->module, "repl_debug.alir");
    }

    long long result = metalir_vm_execute(r->vm, r->module, cfn, &r->sem, NULL, 0);

    if (curr->type == NODE_ASSIGN && ((AssignNode*)curr)->is_implicit_let) {
        VMGlobal *g = r->vm->globals;
        while (g) {
            if (streq(g->name, ((AssignNode*)curr)->name)) {
                if (fn->ret_type.array_size > 0) {
                    g->ptr_val = (void*)(intptr_t)result;
                } else if (fn->ret_type.base == TYPE_CLASS && fn->ret_type.ptr_depth == 0) {
                    int struct_size = 1024;
                    if (r->module && fn->ret_type.class_name) {
                        struct_size = alir_get_struct_size(r->module, fn->ret_type.class_name);
                        if (struct_size < 8) struct_size = 8;
                    }
                    memcpy(g->ptr_val, (void*)(intptr_t)result, struct_size);
                } else {
                    *((long long*)g->ptr_val) = result;
                }
                break;
            }
            g = g->next;
        }
        if (!g) {
            VMGlobal *vg = arena_alloc(&r->vm_arena, sizeof(VMGlobal));
            vg->name = arena_strdup(&r->vm_arena, ((AssignNode*)curr)->name);
            if (fn->ret_type.array_size > 0) {
                vg->ptr_val = (void*)(intptr_t)result;
            } else if (fn->ret_type.base == TYPE_CLASS && fn->ret_type.ptr_depth == 0) {
                vg->ptr_val = arena_alloc(&r->vm_arena, 1024);
                int struct_size = 1024;
                if (r->module && fn->ret_type.class_name) {
                    struct_size = alir_get_struct_size(r->module, fn->ret_type.class_name);
                    if (struct_size < 8) struct_size = 8;
                }
                memcpy(vg->ptr_val, (void*)(intptr_t)result, struct_size);
            } else {
                vg->ptr_val = arena_alloc(&r->vm_arena, 1024);
                *((long long*)vg->ptr_val) = result;
            }
            vg->next = r->vm->globals;
            r->vm->globals = vg;
        }
    }

    if (print_rich) {
        metalir_print_repl_value(fn->ret_type, result);
    }

    return result;
}

long long metalir_execute_parse(MetalirRunner *r, ASTNode *root,
                                const char *source, const char *filename) {
    r->sem.current_source = source;
    r->sem.current_filename = filename;
    int errs = sem_check_program(&r->sem, root);
    if (errs > 0) {
        r->ctx.semantic_error_count = 0;
        r->ctx.error_count = 0;
        return 0;
    }

    metalir_alir_generate(r, root);

    long long result = 0;
    ASTNode *curr = root;
    int id = 0;
    while (curr) {
        if (curr->type == NODE_VAR_DECL) {
            metalir_run_var_decl(r, (VarDeclNode*)curr, id++);
        } else if (curr->type == NODE_LINK) {
            metalir_run_link(r, (LinkNode*)curr);
        } else if (curr->type == NODE_CLASS) {
            metalir_run_class(r, curr, root);
        } else if (curr->type == NODE_FUNC_DEF) {
            metalir_run_func_def(r, curr);
        } else if (curr->type == NODE_LINK) {
            metalir_run_link(r, (LinkNode*)curr);
        } else if (curr->type == NODE_META || curr->type == NODE_POSTMETA) {
        } else if (curr->type != NODE_NAMESPACE && curr->type != NODE_ROOT &&
                   curr->type != NODE_ENUM && curr->type != NODE_ERRNUM &&
                   curr->type != NODE_IMPORT) {
            result = metalir_run_expr(r, curr, id++, 0, NULL);
        }
        curr = curr->next;
    }
    return result;
}

long long metalir_execute_string(MetalirRunner *r, const char *source,
                                 const char *filename) {
    ASTNode *root = metalir_parse(r, source, filename, NULL);
    if (!root || r->parser.has_error) return 0;
    ASTNode *c = root;
    while(c) { debug_metalir("Parsed node type %d\n", c->type); c = c->next; }
    metalir_resolve_imports(r, &root);
    c = root;
    while(c) { debug_metalir("Resolved node type %d\n", c->type); c = c->next; }
    return metalir_execute_parse(r, root, source, filename);
}

int metalir_load_module(MetalirRunner *r, const char *path) {
    char *src = read_file(path);
    int from_malloc = 0;
    if (!src) {
        src = read_import_file(&r->parser, path);
    } else {
        from_malloc = 1;
    }
    if (!src) { debug_metalir("Failed to read %s\n", path); return -1; }
    debug_metalir("Executing module %s\n", path);
    metalir_execute_string(r, src, path);
    if (from_malloc) free(src);
    r->ctx.semantic_error_count = 0;
    r->ctx.error_count = 0;
    return 0;
}

void metalir_resolve_imports(MetalirRunner *r, ASTNode **root) {
    resolve_imports(&r->parser, root);
}
