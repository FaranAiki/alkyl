/**
 * @file generate.c
 * @brief MLIR generation implementation.
 */
#include "mlir/generate.h"
#include "common/common.h"
#include "common/hashmap.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static HashMap mlir_class_map;

/**
 * @brief Re-mangle an inherited method name for a target class.
 * @param original_mangled The original mangled name.
 * @param method_name The method name.
 * @param target_class_name The target class name.
 * @return The re-mangled name, or NULL on failure.
 */
static char* mlir_re_mangle_inherited(const char *original_mangled, const char *method_name, const char *target_class_name) {
    if (!original_mangled || !target_class_name || !method_name) {
        return NULL;
    }

    char search_str[256];
    snprintf(search_str, sizeof(search_str), "_%s", method_name);
    const char *pos = strstr(original_mangled, search_str);
    if (!pos) return NULL;

    const char *class_start = pos;
    while (class_start > original_mangled && *(class_start - 1) != '_') {
        class_start--;
    }

    size_t prefix_len = (size_t)(class_start - original_mangled);
    size_t suffix_len = strlen(pos);
    size_t target_len = strlen(target_class_name);

    char *result = malloc(prefix_len + target_len + suffix_len + 1);
    if (!result) return NULL;

    memcpy(result, original_mangled, prefix_len);
    memcpy(result + prefix_len, target_class_name, target_len);
    memcpy(result + prefix_len + target_len, pos, suffix_len);
    result[prefix_len + target_len + suffix_len] = '\0';

    return result;
}

/**
 * @brief Generate MLIR wrappers for inherited methods from parent classes/traits.
 * @param ctx The MLIR context.
 * @param mod The MLIR module.
 * @param cn The current class node.
 * @param target_class_name The target class name for mangling.
 * @param target_node The target class AST node.
 */
static void generate_inherited_methods(AlkylMlirContext ctx, AlkylMlirModule mod, ClassNode *cn, const char *target_class_name, ClassNode *target_node) {
    if (!cn) return;

    if (cn->parent_name) {
        ClassNode *pcn = hashmap_get(&mlir_class_map, cn->parent_name);
        if (pcn) {
            generate_inherited_methods(ctx, mod, pcn, target_class_name, target_node);

            ASTNode *mem = pcn->members;
            while (mem) {
                if (mem->type == NODE_FUNC_DEF) {
                    FuncDefNode *fn = (FuncDefNode*)mem;
                    if (!fn->is_macro && !streq_lit(fn->name, pcn->name) && !streq_lit(fn->name, "init")) {
                        int overridden = 0;
                        if (target_node) {
                            ASTNode *tmem = target_node->members;
                            while (tmem) {
                                if (tmem->type == NODE_FUNC_DEF && streq_lit(((FuncDefNode*)tmem)->name, fn->name)) {
                                    overridden = 1;
                                    break;
                                }
                                tmem = tmem->next;
                            }
                        }
                        if (!overridden) {
                            char func_name[512];
                            if (fn->mangled_name && target_class_name && fn->class_name && !streq_lit(target_class_name, fn->class_name)) {
                                char *mangled = mlir_re_mangle_inherited(fn->mangled_name, fn->name, target_class_name);
                                if (mangled) {
                                    snprintf(func_name, sizeof(func_name), "%s", mangled);
                                    free(mangled);
                                } else {
                                    snprintf(func_name, sizeof(func_name), "%s", fn->mangled_name);
                                }
                            } else {
                                snprintf(func_name, sizeof(func_name), "%s", fn->mangled_name ? fn->mangled_name : fn->name);
                            }
                            printf("  Mapping Class Method: %s\n", func_name);

                            int num_args = 0;
                            Parameter *p = fn->params;
                            while (p) { num_args++; p = p->next; }
                            num_args++;

                            AlkylMlirFunc mlir_fn = alkyl_mlir_add_function(ctx, mod, func_name, fn->is_extern || fn->body == NULL, num_args);
                            if (fn->body && !fn->is_extern) {
                                AlkylMlirBlock entry_block = alkyl_mlir_add_block(mlir_fn);
                                alkyl_mlir_set_insertion_point_to_end(ctx, entry_block);

                                extern HashMap *mlir_vars;
                                if (!mlir_vars) {
                                    mlir_vars = (HashMap*)malloc(sizeof(HashMap));
                                    hashmap_init(mlir_vars, NULL, 1024);
                                }

                                int arg_idx = 0;
                                AlkylMlirValue this_arg = alkyl_mlir_get_arg(mlir_fn, arg_idx++);
                                AlkylMlirValue this_alloca = alkyl_mlir_build_alloca(ctx, "this");
                                hashmap_put(mlir_vars, "this", this_alloca);
                                alkyl_mlir_build_store(ctx, this_arg, this_alloca);

                                Parameter *p = fn->params;
                                while (p) {
                                    AlkylMlirValue arg_val = alkyl_mlir_get_arg(mlir_fn, arg_idx++);
                                    AlkylMlirValue arg_alloca = alkyl_mlir_build_alloca(ctx, p->name);
                                    hashmap_put(mlir_vars, p->name, arg_alloca);
                                    alkyl_mlir_build_store(ctx, arg_val, arg_alloca);
                                    p = p->next;
                                }

                                reset_mlir_defers();
                                ASTNode *curr = fn->body;
                                while (curr) {
                                    mlir_gen_stmt(ctx, mod, curr);
                                    curr = curr->next;
                                }
                                if (!alkyl_mlir_is_terminated(ctx)) {
                                    execute_mlir_defers(ctx, mod);
                                    alkyl_mlir_build_return(ctx, alkyl_mlir_build_int_constant(ctx, 0));
                                }
                            }
                        }
                    }
                }
                mem = mem->next;
            }
        }
    }

    for (int i = 0; i < cn->traits.count; i++) {
        ClassNode *tcn = hashmap_get(&mlir_class_map, cn->traits.names[i]);
        if (tcn) {
            generate_inherited_methods(ctx, mod, tcn, target_class_name, target_node);

            ASTNode *mem = tcn->members;
            while (mem) {
                if (mem->type == NODE_FUNC_DEF) {
                    FuncDefNode *fn = (FuncDefNode*)mem;
                    if (!fn->is_macro && !streq_lit(fn->name, tcn->name) && !streq_lit(fn->name, "init")) {
                        int overridden = 0;
                        if (target_node) {
                            ASTNode *tmem = target_node->members;
                            while (tmem) {
                                if (tmem->type == NODE_FUNC_DEF && streq_lit(((FuncDefNode*)tmem)->name, fn->name)) {
                                    overridden = 1;
                                    break;
                                }
                                tmem = tmem->next;
                            }
                        }
                        if (!overridden) {
                            char func_name[512];
                            if (fn->mangled_name && target_class_name && fn->class_name && !streq_lit(target_class_name, fn->class_name)) {
                                char *mangled = mlir_re_mangle_inherited(fn->mangled_name, fn->name, target_class_name);
                                if (mangled) {
                                    snprintf(func_name, sizeof(func_name), "%s", mangled);
                                    free(mangled);
                                } else {
                                    snprintf(func_name, sizeof(func_name), "%s", fn->mangled_name);
                                }
                            } else {
                                snprintf(func_name, sizeof(func_name), "%s", fn->mangled_name ? fn->mangled_name : fn->name);
                            }
                            printf("  Mapping Class Method: %s\n", func_name);

                            int num_args = 0;
                            Parameter *p = fn->params;
                            while (p) { num_args++; p = p->next; }
                            num_args++;

                            AlkylMlirFunc mlir_fn = alkyl_mlir_add_function(ctx, mod, func_name, fn->is_extern || fn->body == NULL, num_args);
                            if (fn->body && !fn->is_extern) {
                                AlkylMlirBlock entry_block = alkyl_mlir_add_block(mlir_fn);
                                alkyl_mlir_set_insertion_point_to_end(ctx, entry_block);

                                extern HashMap *mlir_vars;
                                if (!mlir_vars) {
                                    mlir_vars = (HashMap*)malloc(sizeof(HashMap));
                                    hashmap_init(mlir_vars, NULL, 1024);
                                }

                                int arg_idx = 0;
                                AlkylMlirValue this_arg = alkyl_mlir_get_arg(mlir_fn, arg_idx++);
                                AlkylMlirValue this_alloca = alkyl_mlir_build_alloca(ctx, "this");
                                hashmap_put(mlir_vars, "this", this_alloca);
                                alkyl_mlir_build_store(ctx, this_arg, this_alloca);

                                Parameter *p = fn->params;
                                while (p) {
                                    AlkylMlirValue arg_val = alkyl_mlir_get_arg(mlir_fn, arg_idx++);
                                    AlkylMlirValue arg_alloca = alkyl_mlir_build_alloca(ctx, p->name);
                                    hashmap_put(mlir_vars, p->name, arg_alloca);
                                    alkyl_mlir_build_store(ctx, arg_val, arg_alloca);
                                    p = p->next;
                                }

                                reset_mlir_defers();
                                ASTNode *curr = fn->body;
                                while (curr) {
                                    mlir_gen_stmt(ctx, mod, curr);
                                    curr = curr->next;
                                }
                                if (!alkyl_mlir_is_terminated(ctx)) {
                                    execute_mlir_defers(ctx, mod);
                                    alkyl_mlir_build_return(ctx, alkyl_mlir_build_int_constant(ctx, 0));
                                }
                            }
                        }
                    }
                }
                mem = mem->next;
            }
        }
    }
}

/**
 * @brief Generate MLIR code for an AST node (function, class, namespace, etc.).
 * @param ctx The MLIR context.
 * @param mod The MLIR module.
 * @param node The AST node to generate.
 */
static void generate_node(AlkylMlirContext ctx, AlkylMlirModule mod, ASTNode *node) {
    if (!node) return;

    if (node->type == NODE_FUNC_DEF) {
        FuncDefNode *fn = (FuncDefNode*)node;
        printf("  Mapping FuncDefNode: %s\n", fn->name);

        int num_args = 0;
        Parameter *p = fn->params;
        while (p) { num_args++; p = p->next; }
        if (fn->class_name) num_args++;

        AlkylMlirFunc mlir_fn = alkyl_mlir_add_function(ctx, mod, fn->name, fn->is_extern || fn->body == NULL, num_args);
        if (fn->body && !fn->is_extern) {
            AlkylMlirBlock entry_block = alkyl_mlir_add_block(mlir_fn);
            alkyl_mlir_set_insertion_point_to_end(ctx, entry_block);

            extern HashMap *mlir_vars;
            if (!mlir_vars) {
                mlir_vars = (HashMap*)malloc(sizeof(HashMap));
                hashmap_init(mlir_vars, NULL, 1024);
            }

            int arg_idx = 0;
            if (fn->class_name && !fn->is_static) {
                AlkylMlirValue this_arg = alkyl_mlir_get_arg(mlir_fn, arg_idx++);
                AlkylMlirValue this_alloca = alkyl_mlir_build_alloca(ctx, "this");
                hashmap_put(mlir_vars, "this", this_alloca);
                alkyl_mlir_build_store(ctx, this_arg, this_alloca);
            }

            p = fn->params;
            while (p) {
                AlkylMlirValue arg_val = alkyl_mlir_get_arg(mlir_fn, arg_idx++);
                AlkylMlirValue arg_alloca = alkyl_mlir_build_alloca(ctx, p->name);
                hashmap_put(mlir_vars, p->name, arg_alloca);
                alkyl_mlir_build_store(ctx, arg_val, arg_alloca);
                p = p->next;
            }

            reset_mlir_defers();
            ASTNode *curr = fn->body;
            while (curr) {
                mlir_gen_stmt(ctx, mod, curr);
                curr = curr->next;
            }
            if (!alkyl_mlir_is_terminated(ctx)) {
                execute_mlir_defers(ctx, mod);
                alkyl_mlir_build_return(ctx, alkyl_mlir_build_int_constant(ctx, 0));
            }
        }
    } else if (node->type == NODE_CLASS) {
        ClassNode *cls = (ClassNode*)node;
        ASTNode *member = cls->members;
        while (member) {
            if (member->type == NODE_FUNC_DEF) {
                FuncDefNode *fn = (FuncDefNode*)member;
                if (!fn->is_macro) {
                    char* func_name = fn->mangled_name ? fn->mangled_name : fn->name;
                    printf("  Mapping Class Method: %s\n", func_name);

                    int num_args = 0;
                    Parameter *p = fn->params;
                    while (p) { num_args++; p = p->next; }
                    num_args++;

                    AlkylMlirFunc mlir_fn = alkyl_mlir_add_function(ctx, mod, func_name, fn->is_extern || fn->body == NULL, num_args);
                    if (fn->body && !fn->is_extern) {
                        AlkylMlirBlock entry_block = alkyl_mlir_add_block(mlir_fn);
                        alkyl_mlir_set_insertion_point_to_end(ctx, entry_block);

                        extern HashMap *mlir_vars;
                        if (!mlir_vars) {
                            mlir_vars = (HashMap*)malloc(sizeof(HashMap));
                            hashmap_init(mlir_vars, NULL, 1024);
                        }

                        int arg_idx = 0;
                        AlkylMlirValue this_arg = alkyl_mlir_get_arg(mlir_fn, arg_idx++);
                        AlkylMlirValue this_alloca = alkyl_mlir_build_alloca(ctx, "this");
                        hashmap_put(mlir_vars, "this", this_alloca);
                        alkyl_mlir_build_store(ctx, this_arg, this_alloca);

                        Parameter *p = fn->params;
                        while (p) {
                            AlkylMlirValue arg_val = alkyl_mlir_get_arg(mlir_fn, arg_idx++);
                            AlkylMlirValue arg_alloca = alkyl_mlir_build_alloca(ctx, p->name);
                            hashmap_put(mlir_vars, p->name, arg_alloca);
                            alkyl_mlir_build_store(ctx, arg_val, arg_alloca);
                            p = p->next;
                        }

                        reset_mlir_defers();
                        ASTNode *curr = fn->body;
                        while (curr) {
                            mlir_gen_stmt(ctx, mod, curr);
                            curr = curr->next;
                        }
                        if (!alkyl_mlir_is_terminated(ctx)) {
                            execute_mlir_defers(ctx, mod);
                            alkyl_mlir_build_return(ctx, alkyl_mlir_build_int_constant(ctx, 0));
                        }
                    }
                }
            }
            member = member->next;
        }

        generate_inherited_methods(ctx, mod, cls, cls->name, cls);

        for (int i = 0; i < cls->traits.count; i++) {
            ClassNode *tcn = hashmap_get(&mlir_class_map, cls->traits.names[i]);
            if (tcn) {
                generate_inherited_methods(ctx, mod, tcn, cls->name, cls);
            }
        }
    } else if (node->type == NODE_NAMESPACE) {
        NamespaceNode *ns = (NamespaceNode*)node;
        generate_node(ctx, mod, ns->body);
    } else if (node->type == NODE_IMPORT) {
        ImportNode *in = (ImportNode*)node;
        if (in->resolved_body) {
            generate_node(ctx, mod, in->resolved_body);
        }
    }

    generate_node(ctx, mod, node->next);
}

/**
 * @brief Scan the AST and register all classes in the MLIR class map.
 * @param root The root AST node.
 */
static void scan_classes(ASTNode *root) {
    hashmap_init(&mlir_class_map, NULL, 64);
    ASTNode *curr = root;
    while (curr) {
        if (curr->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)curr;
            hashmap_put(&mlir_class_map, cn->name, cn);
        } else if (curr->type == NODE_NAMESPACE) {
            NamespaceNode *ns = (NamespaceNode*)curr;
            scan_classes(ns->body);
        } else if (curr->type == NODE_IMPORT) {
            ImportNode *in = (ImportNode*)curr;
            if (in->resolved_body) {
                scan_classes(in->resolved_body);
            }
        }
        curr = curr->next;
    }
}

// TODO: Do not use global here
ASTNode *mlir_global_ast_root = NULL;
HashMap *mlir_vars = NULL;

/**
 * @brief Generate MLIR output for an entire AST.
 * @param root The root AST node.
 * @param basename Base name for the output .mlir file.
 */
void mlir_generate(ASTNode *root, const char *basename) {
    mlir_global_ast_root = root;
    mlir_vars = malloc(sizeof(HashMap));
    if (!mlir_vars) {
        fprintf(stderr, "Failed to allocate memory for MLIR variables\n");
        return;
    }
    hashmap_init(mlir_vars, NULL, 16);

    scan_classes(root);

    printf("Starting MLIR AST-lowering for %s...\n", basename);
    AlkylMlirContext ctx = alkyl_mlir_create_context();

    if (!ctx) {
        printf("MLIR is not fully linked/available, or create_context failed.\n");
        return;
    }

    AlkylMlirModule mod = alkyl_mlir_create_module(ctx, basename);
    printf("Successfully created MLIR context and module from AST via C++ wrapper!\n");

    if (root) {
        generate_node(ctx, mod, root);
    }

    char out_filename[512];
    snprintf(out_filename, sizeof(out_filename), "%s.mlir", basename);
    alkyl_mlir_dump_module(mod, out_filename);

    alkyl_mlir_destroy_context(ctx);
}
