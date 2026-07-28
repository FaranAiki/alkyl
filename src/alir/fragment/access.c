#include "alir.h"

int alir_get_type_size(VarType t) {
    // All pointers are 8 bytes on a 64-bit architecture
    if (t.ptr_depth > 0) return 8;

    // TODO fix this
    if (t.array_size > 0) {
        return t.array_size * 8;
    }

    switch (t.base) {
        case TYPE_VOID: return 0;
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_UNSIGNED_CHAR: return 1;
        case TYPE_INT:
        case TYPE_UNSIGNED_INT:
        case TYPE_SINGLE: return 4;
        case TYPE_LONG:
        case TYPE_DOUBLE: return 8;
        // TODO: Struct/Class size calculation goes here later
        default: return 8; // Safe fallback
    }
}

static int alir_get_type_align(VarType t) {
    if (t.ptr_depth > 0) return 8;
    if (t.array_size > 0) return 8;
    switch (t.base) {
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_UNSIGNED_CHAR: return 1;
        case TYPE_SHORT: return 2;
        case TYPE_INT:
        case TYPE_UNSIGNED_INT:
        case TYPE_SINGLE:
        case TYPE_ENUM: return 4;
        case TYPE_LONG:
        case TYPE_LONG_LONG:
        case TYPE_UNSIGNED_LONG:
        case TYPE_UNSIGNED_LONG_LONG:
        case TYPE_DOUBLE:
        case TYPE_LONG_DOUBLE: return 8;
        default: return 8;
    }
}

int alir_get_struct_size(AlirModule *mod, const char *struct_name) {
    AlirStruct *st = alir_find_struct(mod, struct_name);
    if (!st || !st->fields) return 8;

    int offset = 0;
    AlirField *f = st->fields;
    while (f) {
        int size = alir_get_type_size(f->type);
        int align = alir_get_type_align(f->type);
        offset = (offset + align - 1) & ~(align - 1);
        offset += size;
        f = f->next;
    }
    return (offset + 7) & ~7;
}

int alir_robust_get_field_index(AlirCtx *ctx, const char *hint_class, const char *field_name) {
    int idx = -1;
    if (hint_class) {
        idx = alir_get_field_index(ctx->module, hint_class, field_name);
    }
    if (idx == -1) {
        AlirStruct *search = ctx->module->structs;
        while (search) {
            AlirField *f = search->fields;
            while(f) {
                if (strcmp(f->name, field_name) == 0) return f->index;
                f = f->next;
            }
            search = search->next;
        }
    }
    return idx == -1 ? 0 : idx;
}

// Handles L-Values: Returns the memory address of arr[index]
AlirValue* alir_gen_addr_index_access(AlirCtx *ctx, IndexAccessNode *aa) {
    // 1. Get the address of the array variable itself
    AlirValue *base_ptr = alir_gen_addr(ctx, aa->target);
    if (!base_ptr) return NULL;

    VarType alir_type = base_ptr->type;

    // 2. CRITICAL: If target is a dynamic pointer (like from malloc),
    // we must load the heap address FROM the stack variable before indexing!
    // We look at the actual ALIR variable type to determine if it's decayed
    // to a pointer instead of static inline representation on the stack.
    if (alir_type.ptr_depth > 0 && alir_type.array_size == 0) {
        AlirValue *loaded_base = new_temp(ctx, alir_type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, loaded_base, base_ptr, NULL));
        base_ptr = loaded_base; // Now base_ptr holds the malloc result
    }

    // 3. Evaluate the index
    AlirValue *index = NULL;
    VarType elem_t = sem_get_node_type(ctx->sem, (ASTNode*)aa);
    VarType target_t = sem_get_node_type(ctx->sem, aa->target);

    // Check if it's a trait access: trait access is represented as ArrayAccess but the result type
    // is a different class than the target type
    if (elem_t.base == TYPE_CLASS && target_t.base == TYPE_CLASS &&
        elem_t.class_name && target_t.class_name &&
        strcmp(elem_t.class_name, target_t.class_name) != 0) {

        VarType ptr_t = elem_t;
        ptr_t.ptr_depth++;

        AlirValue *elem_ptr = new_temp(ctx, ptr_t);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_BITCAST, elem_ptr, base_ptr, NULL));
        return elem_ptr;
    }

    index = alir_gen_expr(ctx, aa->index);
    if (!index) index = alir_const_int(ctx->module, 0);

    // 4. Calculate the type of the pointer we are creating
    elem_t = sem_get_node_type(ctx->sem, (ASTNode*)aa);
    VarType ptr_t = elem_t;
    ptr_t.ptr_depth++;

    // 5. Emit GET_PTR (LLVM getelementptr) and RETURN THE POINTER
    AlirValue *elem_ptr = new_temp(ctx, ptr_t);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, elem_ptr, base_ptr, index));

    // In Alkyl, arrays of arrays are implemented as arrays of pointers.
    // If the element type is an array, we must load the pointer!
    if (elem_t.array_size > 0 || elem_t.array_depth > 0) {
        AlirValue *loaded = new_temp(ctx, ptr_t);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, loaded, elem_ptr, NULL));
        elem_ptr = loaded;
    }

    return elem_ptr;
}

// Handles R-Values: Returns the actual data inside arr[index]
AlirValue* alir_gen_expr_index_access(AlirCtx *ctx, IndexAccessNode *aa) {
    VarType elem_t = sem_get_node_type(ctx->sem, (ASTNode*)aa);
    VarType target_t = sem_get_node_type(ctx->sem, aa->target);

    // Check if it's a trait access. If so, just return the target bitcasted!
    if (elem_t.base == TYPE_CLASS && target_t.base == TYPE_CLASS &&
        elem_t.class_name && target_t.class_name &&
        strcmp(elem_t.class_name, target_t.class_name) != 0) {

        AlirValue *target_val = alir_gen_expr(ctx, aa->target);
        if (!target_val) return NULL;

        AlirValue *casted = new_temp(ctx, elem_t);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_BITCAST, casted, target_val, NULL));
        return casted;
    }

    // 1. Get the memory address of the element
    AlirValue *elem_ptr = alir_gen_addr_index_access(ctx, aa);
    if (!elem_ptr) return NULL;

    // In Alkyl, arrays are manipulated as pointers.
    // If the expression type is an array, its R-value is just the pointer!
    if (elem_t.array_size > 0) {
        return elem_ptr;
    }

    // 2. Emit a LOAD instruction to read the actual value
    AlirValue *loaded_val = new_temp(ctx, elem_t);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, loaded_val, elem_ptr, NULL));
    return loaded_val;
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

        // Special Namespace Handling: If member access resolves to a Namespace Const
        if (obj_t.base == TYPE_NAMESPACE && ma->object->type == NODE_VAR_REF) {
            SemSymbol *ns_sym = sem_symbol_lookup(ctx->sem, ((VarRefNode*)ma->object)->name, NULL);
            if (ns_sym && ns_sym->inner_scope) {
                SemSymbol *member_sym = ns_sym->inner_scope->symbols;
                while (member_sym) {
                    if (strcmp(member_sym->name, ma->member_name) == 0 && member_sym->kind == SYM_VAR) {
                        AlirConstFoldEntry *fold = ctx->const_folds;
                        while (fold) {
                            if (strcmp(fold->name, ma->member_name) == 0) {
                                return fold->value;
                            }
                            fold = fold->next;
                        }
                        fold = ctx->module->const_folds;
                        while (fold) {
                            if (strcmp(fold->name, ma->member_name) == 0) {
                                return fold->value;
                            }
                            fold = fold->next;
                        }
                    }
                    member_sym = member_sym->next;
                }
            }
        }
    }

    AlirValue *ptr = alir_gen_addr(ctx, node);
    if (!ptr) return NULL;

    VarType t = sem_get_node_type(ctx->sem, node);

    // [FIX] ALWAYS trust GET_PTR's physical type over Semantic Analyzer inference bounds.
    // Address returned here is a GET_PTR so it represents a T*. We dynamically extract T.
    if (ptr->type.base != TYPE_UNKNOWN && ptr->type.base != TYPE_AUTO) {
        t = ptr->type;
        if (t.ptr_depth > 0) t.ptr_depth--;
    }

    AlirValue *val = new_temp(ctx, t);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, val, ptr, NULL));
    return val;
}
