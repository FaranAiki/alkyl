with open('src/semantic/modifier/class.c', 'r') as f:
    content = f.read()

old_code = """        } else if (mem->type == NODE_CLASS) {
            ASTNode *next_node = mem->next;
            mem->next = NULL;
            
            // Hoist nested classes (like C anonymous structs) to the parent scope
            SemScope *saved = ctx->current_scope;
            ctx->current_scope = saved->parent;
            sem_scan_top_level(ctx, mem);
            ctx->current_scope = saved;
            
            mem->next = next_node;
        }"""
new_code = """        } else if (mem->type == NODE_CLASS) {
            ASTNode *next_node = mem->next;
            mem->next = NULL;
            
            // Hoist nested classes (like C anonymous structs) to the parent scope
            SemScope *saved = ctx->current_scope;
            ctx->current_scope = saved->parent;
            sem_scan_top_level(ctx, mem);
            ctx->current_scope = saved;
            
            ClassNode *inner_cn = (ClassNode*)mem;
            if (inner_cn->name && strncmp(inner_cn->name, "__anonymous_struct_", 19) == 0) {
                // It's an anonymous struct/union! Inline its fields into the current class scope!
                ASTNode *inner_mem = inner_cn->members;
                while (inner_mem) {
                    if (inner_mem->type == NODE_VAR_DECL) {
                        sem_symbolic_var_decl(ctx, inner_mem);
                    }
                    inner_mem = inner_mem->next;
                }
            }
            
            mem->next = next_node;
        }"""

if old_code in content:
    content = content.replace(old_code, new_code)
    with open('src/semantic/modifier/class.c', 'w') as f:
        f.write(content)
    print("Fixed sem_scan_class_members")
else:
    print("Could not find old_code")
