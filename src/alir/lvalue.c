#include "alir.h"

AlirValue* alir_gen_addr(AlirCtx *ctx, ASTNode *node) {
    if (node->type == NODE_VAR_REF) {
        VarRefNode *vn = (VarRefNode*)node;
        
        // Handle Implicit Member Access (this.x)
        if (vn->is_class_member) {
            // Get 'this' pointer
            AlirSymbol *this_sym = alir_find_symbol(ctx, "this");
            if (!this_sym) return NULL; // Should not happen if sem check passed
            
            // Get Class Name from 'this' type
            char *class_name = this_sym->type.class_name;
            if (!class_name) return NULL;
            
            // Resolve field index
            int idx = alir_get_field_index(ctx->module, class_name, vn->name);
            if (idx == -1) return NULL;
            
            // Generate GEP
            // Note: this_sym->ptr is the address where 'this' is stored (e.g. stack param addr).
            // We need to load 'this' (Class*) first.
            AlirValue *this_ptr = new_temp(ctx, this_sym->type);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, this_ptr, this_sym->ptr, NULL));
            
            // Now get address of member
            // Result is pointer to member type
            // Need precise member type from struct registry or sem ctx
            VarType mem_type = sem_get_node_type(ctx->sem, node);
            mem_type.ptr_depth++; // return pointer
            
            AlirValue *res = new_temp(ctx, mem_type); 
            emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, res, this_ptr, alir_const_int(ctx->module, idx)));
            return res;
        }

        // Check IR local symbols first
        AlirSymbol *sym = alir_find_symbol(ctx, vn->name);
        if (sym) return sym->ptr;
        // If not found locally, assume global
        return alir_val_var(ctx->module, vn->name);
    }
    
    if (node->type == NODE_MEMBER_ACCESS) {
        MemberAccessNode *ma = (MemberAccessNode*)node;
        
        // Check if this is an Enum Access first. Enums are constants, not L-values in memory.
        VarType obj_t = sem_get_node_type(ctx->sem, ma->object);
        if (obj_t.base == TYPE_ENUM) {
             // Cannot get address of an enum constant
             return NULL; 
        }

        AlirValue *base_ptr = alir_gen_addr(ctx, ma->object);
        if (!base_ptr) base_ptr = alir_gen_expr(ctx, ma->object);

        if (obj_t.class_name) {
            int idx = alir_get_field_index(ctx->module, obj_t.class_name, ma->member_name);
            if (idx != -1) {
                AlirValue *res = new_temp(ctx, (VarType){TYPE_INT, 1}); // Pointer to int? Should be pointer to field type
                // In full implementation, lookup field type from struct registry
                emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, res, base_ptr, alir_const_int(ctx->module, idx)));
                return res;
            }
        }
    }
    
    if (node->type == NODE_ARRAY_ACCESS) {
        ArrayAccessNode *aa = (ArrayAccessNode*)node;
        AlirValue *base_ptr = alir_gen_addr(ctx, aa->target);
        AlirValue *index = alir_gen_expr(ctx, aa->index);
        
        // Result is pointer to element
        // We really should know the element type here.
        VarType elem_t = sem_get_node_type(ctx->sem, (ASTNode*)aa);
        elem_t.ptr_depth++; // Make it a pointer (L-Value)

        AlirValue *res = new_temp(ctx, elem_t);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, res, base_ptr, index));
        return res;
    }
    
    return NULL;
}

AlirValue* alir_gen_trait_access(AlirCtx *ctx, TraitAccessNode *ta) {
    AlirValue *base_ptr = alir_gen_addr(ctx, ta->object);
    if (!base_ptr) base_ptr = alir_gen_expr(ctx, ta->object);
    
    VarType obj_t = sem_get_node_type(ctx->sem, ta->object);
    
    // 1. Try to find a field named after the Trait (Mixin strategy)
    if (obj_t.class_name) {
        int idx = alir_get_field_index(ctx->module, obj_t.class_name, ta->trait_name);
        if (idx != -1) {
            // Found explicit field for trait
            AlirValue *res = new_temp(ctx, (VarType){TYPE_CLASS, 1, alir_strdup(ctx->module, ta->trait_name)});
            emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, res, base_ptr, alir_const_int(ctx->module, idx)));
            return res;
        }
    }
    
    // 2. Fallback: Bitcast (Unsafe/Direct Cast)
    VarType trait_ptr_t = {TYPE_CLASS, 1, alir_strdup(ctx->module, ta->trait_name)};
    AlirValue *cast_res = new_temp(ctx, trait_ptr_t);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_BITCAST, cast_res, base_ptr, NULL));
    return cast_res;
}

AlirValue* alir_gen_literal(AlirCtx *ctx, LiteralNode *ln) {
    if (ln->var_type.base == TYPE_INT) return alir_const_int(ctx->module, ln->val.int_val);
    if (ln->var_type.base == TYPE_FLOAT) return alir_const_float(ctx->module, ln->val.double_val);
    if (ln->var_type.base == TYPE_STRING) {
        return alir_module_add_string_literal(ctx->module, ln->val.str_val, ctx->str_counter++);
    }
    // Fallback
    return alir_const_int(ctx->module, 0);
}

AlirValue* alir_gen_var_ref(AlirCtx *ctx, VarRefNode *vn) {
    AlirValue *ptr = alir_gen_addr(ctx, (ASTNode*)vn);
    
    // Get precise type from Semantics
    VarType t = sem_get_node_type(ctx->sem, (ASTNode*)vn);
    
    AlirValue *val = new_temp(ctx, t);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, val, ptr, NULL));
    return val;
}

AlirValue* alir_gen_access(AlirCtx *ctx, ASTNode *node) {
    // Special Enum Handling: If member access resolves to an Enum Type
    if (node->type == NODE_MEMBER_ACCESS) {
        MemberAccessNode *ma = (MemberAccessNode*)node;
        VarType obj_t = sem_get_node_type(ctx->sem, ma->object);
        if (obj_t.base == TYPE_ENUM && obj_t.class_name) {
            long val = 0;
            if (alir_get_enum_value(ctx->module, obj_t.class_name, ma->member_name, &val)) {
                return alir_const_int(ctx->module, val);
            }
        }
    }

    AlirValue *ptr = alir_gen_addr(ctx, node);
    
    VarType t = sem_get_node_type(ctx->sem, node);
    
    AlirValue *val = new_temp(ctx, t); 
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, val, ptr, NULL));
    return val;
}

AlirValue* alir_gen_binary_op(AlirCtx *ctx, BinaryOpNode *bn) {
    AlirValue *l = alir_gen_expr(ctx, bn->left);
    AlirValue *r = alir_gen_expr(ctx, bn->right);
    
    // Check types via Semantic Context to decide on Float vs Int ops
    VarType l_type = sem_get_node_type(ctx->sem, bn->left);
    VarType r_type = sem_get_node_type(ctx->sem, bn->right);

    int is_float = (l_type.base == TYPE_FLOAT || l_type.base == TYPE_DOUBLE ||
                    r_type.base == TYPE_FLOAT || r_type.base == TYPE_DOUBLE);

    if (is_float) {
        VarType target = {TYPE_DOUBLE, 0}; // Default to double for mixed
        l = promote(ctx, l, target);
        r = promote(ctx, r, target);
    }

    AlirOpcode op = ALIR_OP_ADD;
    switch(bn->op) {
        case TOKEN_PLUS: op = is_float ? ALIR_OP_FADD : ALIR_OP_ADD; break;
        case TOKEN_MINUS: op = is_float ? ALIR_OP_FSUB : ALIR_OP_SUB; break;
        case TOKEN_STAR: op = is_float ? ALIR_OP_FMUL : ALIR_OP_MUL; break;
        case TOKEN_SLASH: op = is_float ? ALIR_OP_FDIV : ALIR_OP_DIV; break;
        case TOKEN_EQ: op = ALIR_OP_EQ; break;
        case TOKEN_LT: op = ALIR_OP_LT; break;
        // ... add other cases
    }
    
    // Result type logic
    VarType res_type = is_float ? (VarType){TYPE_DOUBLE, 0} : (VarType){TYPE_INT, 0};
    if (op == ALIR_OP_EQ || op == ALIR_OP_LT) res_type = (VarType){TYPE_BOOL, 0};
    
    AlirValue *dest = new_temp(ctx, res_type);
    emit(ctx, mk_inst(ctx->module, op, dest, l, r));
    return dest;
}

AlirValue* alir_gen_call_std(AlirCtx *ctx, CallNode *cn) {
    AlirInst *call = mk_inst(ctx->module, ALIR_OP_CALL, NULL, alir_val_var(ctx->module, cn->name), NULL);
    
    int count = 0; ASTNode *a = cn->args; while(a) { count++; a=a->next; }
    call->arg_count = count;
    call->args = alir_alloc(ctx->module, sizeof(AlirValue*) * count);
    
    int i = 0; a = cn->args;
    while(a) {
        call->args[i++] = alir_gen_expr(ctx, a);
        a = a->next;
    }
    
    // Result type from Semantic Table
    VarType ret_type = sem_get_node_type(ctx->sem, (ASTNode*)cn);
    
    AlirValue *dest = new_temp(ctx, ret_type); 
    call->dest = dest;
    emit(ctx, call);
    return dest;
}

AlirValue* alir_gen_call(AlirCtx *ctx, CallNode *cn) {
    // Check if it's a constructor call via Struct Registry
    if (alir_find_struct(ctx->module, cn->name)) {
        return alir_lower_new_object(ctx, cn->name, cn->args);
    }
    return alir_gen_call_std(ctx, cn);
}

AlirValue* alir_gen_method_call(AlirCtx *ctx, MethodCallNode *mc) {
    AlirValue *this_ptr = alir_gen_addr(ctx, mc->object);
    if (!this_ptr) this_ptr = alir_gen_expr(ctx, mc->object); 

    // Mangle: Class_Method
    VarType obj_t = sem_get_node_type(ctx->sem, mc->object);
    char func_name[256];
    if (obj_t.class_name) snprintf(func_name, 256, "%s_%s", obj_t.class_name, mc->method_name);
    else snprintf(func_name, 256, "%s", mc->method_name);

    AlirInst *call = mk_inst(ctx->module, ALIR_OP_CALL, NULL, alir_val_var(ctx->module, func_name), NULL);
    
    int count = 0; ASTNode *a = mc->args; while(a) { count++; a=a->next; }
    call->arg_count = count + 1;
    call->args = alir_alloc(ctx->module, sizeof(AlirValue*) * (count + 1));
    
    call->args[0] = this_ptr;
    int i = 1; a = mc->args;
    while(a) {
        call->args[i++] = alir_gen_expr(ctx, a);
        a = a->next;
    }
    
    VarType ret_type = sem_get_node_type(ctx->sem, (ASTNode*)mc);
    AlirValue *dest = new_temp(ctx, ret_type);
    call->dest = dest;
    emit(ctx, call);
    return dest;
}

AlirValue* alir_gen_expr(AlirCtx *ctx, ASTNode *node) {
    if (!node) return NULL;
    
    ctx->current_line = node->line;
    ctx->current_col = node->col;

    switch(node->type) {
        case NODE_LITERAL: return alir_gen_literal(ctx, (LiteralNode*)node);
        case NODE_VAR_REF: return alir_gen_var_ref(ctx, (VarRefNode*)node);
        case NODE_BINARY_OP: return alir_gen_binary_op(ctx, (BinaryOpNode*)node);
        case NODE_MEMBER_ACCESS: return alir_gen_access(ctx, node);
        case NODE_ARRAY_ACCESS: return alir_gen_access(ctx, node);
        case NODE_CALL: return alir_gen_call(ctx, (CallNode*)node);
        case NODE_METHOD_CALL: return alir_gen_method_call(ctx, (MethodCallNode*)node);
        case NODE_TRAIT_ACCESS: return alir_gen_trait_access(ctx, (TraitAccessNode*)node);
        default: return NULL;
    }
}
