with open('src/semantic/modifier/class.c', 'r') as f:
    content = f.read()

old_code = """                // It's an anonymous struct/union! Inline its fields into the current class scope!
                ASTNode *inner_mem = inner_cn->members;
                while (inner_mem) {
                    if (inner_mem->type == NODE_VAR_DECL) {
                        sem_symbolic_var_decl(ctx, inner_mem);
                    }
                    inner_mem = inner_mem->next;
                }"""
new_code = """                // It's an anonymous struct/union! Inline its fields into the current class scope!
                ASTNode *inner_mem = inner_cn->members;
                while (inner_mem) {
                    if (inner_mem->type == NODE_VAR_DECL) {
                        printf("DEBUG: Inlining anonymous member: %s\\n", ((VarDeclNode*)inner_mem)->name);
                        sem_symbolic_var_decl(ctx, inner_mem);
                    }
                    inner_mem = inner_mem->next;
                }"""

if old_code in content:
    content = content.replace(old_code, new_code)
    with open('src/semantic/modifier/class.c', 'w') as f:
        f.write(content)
    print("Fixed fix6")
else:
    print("Could not find old_code")
