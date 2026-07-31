import sys
with open('src/driver/cli.c', 'r') as f:
    content = f.read()

target = """                VarType chk = sem_get_node_type(&r->sem, curr);
                if (chk.base == TYPE_NAMESPACE || chk.base == TYPE_CLASS) {
                    print_symbol_info(&r->sem, chk);"""

replacement = """                VarType chk = sem_get_node_type(&r->sem, curr);
                int is_type_sym = 0;
                if ((chk.base == TYPE_NAMESPACE || chk.base == TYPE_CLASS) && curr->type == NODE_VAR_REF) {
                    SemSymbol *sym = sem_symbol_lookup(&r->sem, ((VarRefNode*)curr)->name, NULL);
                    if (sym && (sym->kind == SYM_CLASS || sym->kind == SYM_NAMESPACE)) {
                        is_type_sym = 1;
                    }
                }
                if (is_type_sym) {
                    print_symbol_info(&r->sem, chk);"""

content = content.replace(target, replacement)
with open('src/driver/cli.c', 'w') as f:
    f.write(content)
