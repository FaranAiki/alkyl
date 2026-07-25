import re

# 1. Deduplicate strings in ALIR
with open('src/alir/core.c', 'r') as f:
    code = f.read()
new_code = code.replace("""AlirValue* alir_module_add_string_literal(AlirModule *mod, const char *content, VarType type, int id_hint) {
    char label[64];""", """AlirValue* alir_module_add_string_literal(AlirModule *mod, const char *content, VarType type, int id_hint) {
    AlirGlobal *curr = mod->globals;
    while (curr) {
        if (curr->string_content && strcmp(curr->string_content, content) == 0) {
            return alir_val_global(mod, curr->name, curr->type);
        }
        curr = curr->next;
    }
    char label[64];""")
with open('src/alir/core.c', 'w') as f:
    f.write(new_code)

# 2. Return string literal in ALIR lvalue
with open('src/alir/lvalue.c', 'r') as f:
    code = f.read()
new_code = code.replace("""        case NODE_TYPEOF: {
            SizeOfNode *sn = (SizeOfNode*)node;
            VarType op_type;
            if (sn->target_type.base == TYPE_UNKNOWN && sn->operand) {
                op_type = sem_get_node_type(ctx->sem, sn->operand);
            } else {
                op_type = sn->target_type;
            }
            return alir_const_int(ctx->module, op_type.base);
        }""", """        case NODE_TYPEOF: {
            SizeOfNode *sn = (SizeOfNode*)node;
            VarType op_type;
            if (sn->target_type.base == TYPE_UNKNOWN && sn->operand) {
                op_type = sem_get_node_type(ctx->sem, sn->operand);
            } else {
                op_type = sn->target_type;
            }
            VarType str_type = { .base = TYPE_CHAR, .ptr_depth = 1, .array_size = 0 };
            return alir_module_add_string_literal(ctx->module, sem_type_to_str(op_type), str_type, ctx->str_counter++);
        }""")
with open('src/alir/lvalue.c', 'w') as f:
    f.write(new_code)

# 3. Change semantic type to char*
with open('src/semantic/check.c', 'r') as f:
    code = f.read()
new_code = code.replace("""            if (sn->target_type.base == TYPE_UNKNOWN && sn->operand) {
                sem_check_expr(ctx, sn->operand);
            }
            sem_set_node_type(ctx, node, (VarType){ .base = TYPE_INT, .ptr_depth = 0 });""", """            if (sn->target_type.base == TYPE_UNKNOWN && sn->operand) {
                sem_check_expr(ctx, sn->operand);
            }
            sem_set_node_type(ctx, node, (VarType){ .base = TYPE_CHAR, .ptr_depth = 1, .array_size = 0 });""")
with open('src/semantic/check.c', 'w') as f:
    f.write(new_code)
