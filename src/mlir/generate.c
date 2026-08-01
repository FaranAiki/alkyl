#include "mlir/generate.h"
#include "common/hashmap.h"
#include <stdlib.h>
#include <stdio.h>

static void generate_node(AlkylMlirContext ctx, AlkylMlirModule mod, ASTNode *node) {
    if (!node) return;
    
    if (node->type == NODE_FUNC_DEF) {
        FuncDefNode *fn = (FuncDefNode*)node;
        printf("  Mapping FuncDefNode: %s\n", fn->name);
        
        int num_args = 0;
        Parameter *p = fn->params;
        while (p) { num_args++; p = p->next; }
        if (fn->class_name) num_args++; // implicit this
        
        AlkylMlirFunc mlir_fn = alkyl_mlir_add_function(ctx, mod, fn->name, fn->is_extern || fn->body == NULL, num_args);
        if (fn->body && !fn->is_extern) {
            AlkylMlirBlock entry_block = alkyl_mlir_add_block(mlir_fn);
            alkyl_mlir_set_insertion_point_to_end(ctx, entry_block);
            
            // Generate the body statements recursively
            ASTNode *curr = fn->body;
            while (curr) {
                mlir_gen_stmt(ctx, mod, curr);
                curr = curr->next;
            }
            
            // Fallback return if none was explicitly written in the AST (often required in void funcs)
            if (!alkyl_mlir_is_terminated(ctx)) {
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
                    num_args++; // implicit this
                    
                    AlkylMlirFunc mlir_fn = alkyl_mlir_add_function(ctx, mod, func_name, fn->is_extern || fn->body == NULL, num_args);
                    if (fn->body && !fn->is_extern) {
                        AlkylMlirBlock entry_block = alkyl_mlir_add_block(mlir_fn);
                        alkyl_mlir_set_insertion_point_to_end(ctx, entry_block);
                        
                        ASTNode *curr = fn->body;
                        while (curr) {
                            mlir_gen_stmt(ctx, mod, curr);
                            curr = curr->next;
                        }
                        // Fallback return
                        if (!alkyl_mlir_is_terminated(ctx)) {
                            alkyl_mlir_build_return(ctx, alkyl_mlir_build_int_constant(ctx, 0));
                        }
                    }
                }
            }
            member = member->next;
        }
    } else if (node->type == NODE_NAMESPACE) {
        NamespaceNode *ns = (NamespaceNode*)node;
        generate_node(ctx, mod, ns->body);
    }
    
    generate_node(ctx, mod, node->next);
}

ASTNode *mlir_global_ast_root = NULL;
HashMap *mlir_vars = NULL;

void mlir_generate(ASTNode *root, const char *basename) {
    mlir_global_ast_root = root;
    mlir_vars = malloc(sizeof(HashMap));
    hashmap_init(mlir_vars, NULL, 16);
    printf("Starting MLIR AST-lowering for %s...\n", basename);
    AlkylMlirContext ctx = alkyl_mlir_create_context();
    
    if (!ctx) {
        printf("MLIR is not fully linked/available, or create_context failed.\n");
        return;
    }
    
    AlkylMlirModule mod = alkyl_mlir_create_module(ctx, basename);
    printf("Successfully created MLIR context and module from AST via C++ wrapper!\n");
    
    // Traverse root
    if (root) {
        generate_node(ctx, mod, root);
    }
    
    char out_filename[512];
    snprintf(out_filename, sizeof(out_filename), "%s.mlir", basename);
    alkyl_mlir_dump_module(mod, out_filename);
    
    alkyl_mlir_destroy_context(ctx);
}
