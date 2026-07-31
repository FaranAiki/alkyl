#include "mlir/generate.h"
#include <stdio.h>
#include <stdio.h>

static void generate_node(AlkylMlirContext ctx, AlkylMlirModule mod, ASTNode *node) {
    if (!node) return;
    
    if (node->type == NODE_FUNC_DEF) {
        FuncDefNode *fn = (FuncDefNode*)node;
        printf("  Mapping FuncDefNode: %s\n", fn->name);
        
        AlkylMlirFunc mlir_fn = alkyl_mlir_add_function(ctx, mod, fn->name);
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
            alkyl_mlir_build_return(ctx, NULL);
        }
    } else if (node->type == NODE_NAMESPACE) {
        NamespaceNode *ns = (NamespaceNode*)node;
        generate_node(ctx, mod, ns->body);
    }
    
    generate_node(ctx, mod, node->next);
}

void mlir_generate(ASTNode *root, const char *basename) {
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
    
    alkyl_mlir_destroy_context(ctx);
}
