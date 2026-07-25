import re

with open('src/alir/lvalue.c', 'r') as f:
    code = f.read()

# Modify NODE_TYPEOF to return an integer hash
typeof_str = """        case NODE_TYPEOF: {
            SizeOfNode *sn = (SizeOfNode*)node;
            VarType op_type;
            if (sn->target_type.base == TYPE_UNKNOWN && sn->operand) {
                op_type = sem_get_node_type(ctx->sem, sn->operand);
            } else {
                op_type = sn->target_type;
            }
            unsigned int hash = 5381;
            char *str = sem_type_to_str(op_type);
            int c;
            while ((c = *str++)) hash = ((hash << 5) + hash) + c;
            return alir_const_int(ctx->module, hash);
        }"""

code = re.sub(r'case NODE_TYPEOF: \{.*?\}', typeof_str, code, flags=re.DOTALL)

var_ref_addition = """        } else if (sym && sym->kind == SYM_CLASS) {
            unsigned int hash = 5381;
            char *str = sym->name;
            int c;
            while ((c = *str++)) hash = ((hash << 5) + hash) + c;
            return alir_const_int(ctx->module, hash);
        }"""

code = code.replace('} else if (sym && sym->kind == SYM_VAR) {', var_ref_addition + ' else if (sym && sym->kind == SYM_VAR) {')

with open('src/alir/lvalue.c', 'w') as f:
    f.write(code)

