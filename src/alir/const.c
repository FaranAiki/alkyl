#include "alir.h"
#include "../common/hashmap.h"

AlirValue* alir_const_int(AlirModule *mod, long val) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_CONST;
    v->type = (VarType){TYPE_INT, 0};
    v->val.int_val = val;
    return v;
}

AlirValue* alir_const_char(AlirModule *mod, char val) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_CONST;
    v->type = (VarType){TYPE_CHAR, 0};
    v->val.char_val = val;
    return v;
}

AlirValue* alir_const_unsigned_char(AlirModule *mod, unsigned char val) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_CONST;
    v->type = (VarType){TYPE_CHAR, 0};
    v->val.unsigned_char_val = val;
    return v;
}

AlirValue* alir_const_unsigned_int(AlirModule *mod, unsigned int val) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_CONST;
    v->type = (VarType){TYPE_UNSIGNED_INT, 0};
    v->val.unsigned_int_val = val;
    return v;
}

AlirValue* alir_const_long(AlirModule *mod, long val) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_CONST;
    v->type = (VarType){TYPE_LONG, 0};
    v->val.long_val = val;
    return v;
}

AlirValue* alir_const_long_long(AlirModule *mod, long long val) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_CONST;
    v->type = (VarType){TYPE_LONG_LONG, 0};
    v->val.long_long_val = val;
    return v;
}

AlirValue* alir_const_unsigned_long(AlirModule *mod, unsigned long val) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_CONST;
    v->type = (VarType){TYPE_UNSIGNED_LONG, 0};
    v->val.unsigned_long_val = val;
    return v;
}

AlirValue* alir_const_unsigned_long_long(AlirModule *mod, unsigned long long val) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_CONST;
    v->type = (VarType){TYPE_UNSIGNED_LONG_LONG, 0};
    v->val.unsigned_long_val = val;
    return v;
}

AlirValue* alir_const_float(AlirModule *mod, float val) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_CONST;
    v->type = (VarType){TYPE_SINGLE, 0};
    v->val.single_val = val;
    return v;
}

AlirValue* alir_const_double(AlirModule *mod, double val) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_CONST;
    v->type = (VarType){TYPE_DOUBLE, 0};
    v->val.double_val = val;
    return v;
}

AlirValue* alir_val_temp(AlirModule *mod, VarType t, int id) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_TEMP;
    v->type = t;
    v->temp_id = id;
    return v;
}

AlirValue* alir_val_var(AlirModule *mod, const char *name) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_VAR;
    v->val.str_val = alir_strdup(mod, name);
    return v;
}

AlirValue* alir_val_global(AlirModule *mod, const char *name, VarType type) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_GLOBAL;
    v->val.str_val = alir_strdup(mod, name);
    v->type = type;
    return v;
}

AlirValue* alir_val_label(AlirModule *mod, const char *label) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_LABEL;
    v->val.str_val = alir_strdup(mod, label);
    return v;
}

AlirValue* alir_val_type(AlirModule *mod, const char *type_name) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_TYPE;
    v->val.str_val = alir_strdup(mod, type_name);
    v->type = (VarType){TYPE_CLASS, 0, alir_strdup(mod, type_name), 0, 0};
    return v;
}

static AlirValue* fold_const_expr_ahead(AlirCtx *ctx, ASTNode *node);

void alir_register_enum(AlirModule *mod, const char *name, AlirEnumEntry *entries) {
    AlirEnum *e = alir_alloc(mod, sizeof(AlirEnum));
    e->name = alir_strdup(mod, name);
    e->entries = entries;
    e->next = mod->enums;
    mod->enums = e;
    hashmap_put(&mod->enum_map, name, e);
}

AlirEnum* alir_find_enum(AlirModule *mod, const char *name) {
    AlirEnum *e = hashmap_get(&mod->enum_map, name);
    if (e) return e;
    e = mod->enums;
    while(e) {
        if (strcmp(e->name, name) == 0) {
            hashmap_put(&mod->enum_map, name, e);
            return e;
        }
        e = e->next;
    }
    return NULL;
}

int alir_get_enum_value(AlirModule *mod, const char *enum_name, const char *entry_name, long *out_val) {
    AlirEnum *e = alir_find_enum(mod, enum_name);
    if (!e) return 0;

    AlirEnumEntry *ent = e->entries;
    while(ent) {
        if (strcmp(ent->name, entry_name) == 0) {
            *out_val = ent->value;
            return 1;
        }
        ent = ent->next;
    }
    return 0;
}

static AlirValue* fold_literal(AlirCtx *ctx, LiteralNode *ln) {
    VarType t = ln->base.sem_type;
    switch (t.base) {
        case TYPE_INT:
            return alir_const_int(ctx->module, ln->val.int_val);
        case TYPE_CHAR:
            return alir_const_char(ctx->module, (char)ln->val.int_val);
        case TYPE_UNSIGNED_CHAR:
            return alir_const_unsigned_char(ctx->module, (unsigned char)ln->val.int_val);
        case TYPE_UNSIGNED_INT:
            return alir_const_unsigned_int(ctx->module, (unsigned int)ln->val.int_val);
        case TYPE_LONG:
            return alir_const_long(ctx->module, ln->val.long_val);
        case TYPE_LONG_LONG:
            return alir_const_long_long(ctx->module, ln->val.long_long_val);
        case TYPE_UNSIGNED_LONG:
            return alir_const_unsigned_long(ctx->module, (unsigned long)ln->val.int_val);
        case TYPE_UNSIGNED_LONG_LONG:
            return alir_const_unsigned_long_long(ctx->module, (unsigned long long)ln->val.int_val);
        case TYPE_SINGLE:
            return alir_const_float(ctx->module, ln->val.single_val);
        case TYPE_DOUBLE:
            return alir_const_double(ctx->module, ln->val.double_val);
        default:
            return NULL;
    }
}

static AlirValue* fold_binary_op(AlirCtx *ctx, BinaryOpNode *bn) {
    AlirValue *left = fold_const_expr_ahead(ctx, bn->left);
    AlirValue *right = fold_const_expr_ahead(ctx, bn->right);
    if (!left || !right) return NULL;
    if (left->kind != ALIR_VAL_CONST || right->kind != ALIR_VAL_CONST) return NULL;

    bool left_is_float = (left->type.base == TYPE_SINGLE || left->type.base == TYPE_DOUBLE);
    bool right_is_float = (right->type.base == TYPE_SINGLE || right->type.base == TYPE_DOUBLE);
    int op = bn->op;

    if (op == TOKEN_PLUS) {
        if (left_is_float || right_is_float) {
            double lv = (left->type.base == TYPE_DOUBLE) ? left->val.double_val : left->val.single_val;
            double rv = (right->type.base == TYPE_DOUBLE) ? right->val.double_val : right->val.single_val;
            return (left_is_float && left->type.base == TYPE_DOUBLE) ? alir_const_double(ctx->module, lv + rv) : alir_const_float(ctx->module, (float)(lv + rv));
        }
        return alir_const_int(ctx->module, left->val.int_val + right->val.int_val);
    }
    if (op == TOKEN_MINUS) {
        if (left_is_float || right_is_float) {
            double lv = (left->type.base == TYPE_DOUBLE) ? left->val.double_val : left->val.single_val;
            double rv = (right->type.base == TYPE_DOUBLE) ? right->val.double_val : right->val.single_val;
            return (left_is_float && left->type.base == TYPE_DOUBLE) ? alir_const_double(ctx->module, lv - rv) : alir_const_float(ctx->module, (float)(lv - rv));
        }
        return alir_const_int(ctx->module, left->val.int_val - right->val.int_val);
    }
    if (op == TOKEN_STAR) {
        if (left_is_float || right_is_float) {
            double lv = (left->type.base == TYPE_DOUBLE) ? left->val.double_val : left->val.single_val;
            double rv = (right->type.base == TYPE_DOUBLE) ? right->val.double_val : right->val.single_val;
            return (left_is_float && left->type.base == TYPE_DOUBLE) ? alir_const_double(ctx->module, lv * rv) : alir_const_float(ctx->module, (float)(lv * rv));
        }
        return alir_const_int(ctx->module, left->val.int_val * right->val.int_val);
    }
    if (op == TOKEN_SLASH) {
        if (left_is_float || right_is_float) {
            double lv = (left->type.base == TYPE_DOUBLE) ? left->val.double_val : left->val.single_val;
            double rv = (right->type.base == TYPE_DOUBLE) ? right->val.double_val : right->val.single_val;
            if (rv == 0) return NULL;
            return (left_is_float && left->type.base == TYPE_DOUBLE) ? alir_const_double(ctx->module, lv / rv) : alir_const_float(ctx->module, (float)(lv / rv));
        }
        if (right->val.int_val == 0) return NULL;
        return alir_const_int(ctx->module, left->val.int_val / right->val.int_val);
    }
    if (op == TOKEN_MOD) {
        if (right->val.int_val == 0) return NULL;
        return alir_const_int(ctx->module, left->val.int_val % right->val.int_val);
    }
    if (op == TOKEN_AND) {
        return alir_const_int(ctx->module, left->val.int_val & right->val.int_val);
    }
    if (op == TOKEN_OR) {
        return alir_const_int(ctx->module, left->val.int_val | right->val.int_val);
    }
    if (op == TOKEN_XOR) {
        return alir_const_int(ctx->module, left->val.int_val ^ right->val.int_val);
    }
    if (op == TOKEN_LSHIFT) {
        return alir_const_int(ctx->module, left->val.int_val << right->val.int_val);
    }
    if (op == TOKEN_RSHIFT) {
        return alir_const_int(ctx->module, left->val.int_val >> right->val.int_val);
    }
    if (op == TOKEN_LROTATE) {
        unsigned long long x = (unsigned long long)left->val.int_val;
        int n = right->val.int_val & 63;
        unsigned long long result = (x << n) | (x >> (64 - n));
        return alir_const_int(ctx->module, (long)result);
    }
    if (op == TOKEN_RROTATE) {
        unsigned long long x = (unsigned long long)left->val.int_val;
        int n = right->val.int_val & 63;
        unsigned long long result = (x >> n) | (x << (64 - n));
        return alir_const_int(ctx->module, (long)result);
    }
    if (op == TOKEN_EQ) {
        long result = (left->val.int_val == right->val.int_val);
        return alir_const_int(ctx->module, result);
    }
    if (op == TOKEN_NEQ) {
        long result = (left->val.int_val != right->val.int_val);
        return alir_const_int(ctx->module, result);
    }
    if (op == TOKEN_LT) {
        long result = (left->val.int_val < right->val.int_val);
        return alir_const_int(ctx->module, result);
    }
    if (op == TOKEN_GT) {
        long result = (left->val.int_val > right->val.int_val);
        return alir_const_int(ctx->module, result);
    }
    if (op == TOKEN_LTE) {
        long result = (left->val.int_val <= right->val.int_val);
        return alir_const_int(ctx->module, result);
    }
    if (op == TOKEN_GTE) {
        long result = (left->val.int_val >= right->val.int_val);
        return alir_const_int(ctx->module, result);
    }
    if (op == TOKEN_AND_AND) {
        long result = (left->val.int_val && right->val.int_val);
        return alir_const_int(ctx->module, result);
    }
    if (op == TOKEN_OR_OR) {
        long result = (left->val.int_val || right->val.int_val);
        return alir_const_int(ctx->module, result);
    }
    return NULL;
}

static AlirValue* fold_unary_op(AlirCtx *ctx, UnaryOpNode *un) {
    AlirValue *val = fold_const_expr_ahead(ctx, un->operand);
    if (!val || val->kind != ALIR_VAL_CONST) return NULL;

    if (un->op == TOKEN_MINUS) {
        if (val->type.base == TYPE_DOUBLE) return alir_const_double(ctx->module, -val->val.double_val);
        if (val->type.base == TYPE_SINGLE) return alir_const_float(ctx->module, -val->val.single_val);
        return alir_const_int(ctx->module, -val->val.int_val);
    }
    if (un->op == TOKEN_NOT) {
        return alir_const_int(ctx->module, !val->val.int_val);
    }
    if (un->op == TOKEN_BIT_NOT) {
        return alir_const_int(ctx->module, ~val->val.int_val);
    }
    return NULL;
}

static AlirValue* fold_const_expr_ahead(AlirCtx *ctx, ASTNode *node) {
    if (!node) return NULL;

    switch (node->type) {
        case NODE_LITERAL:
            return fold_literal(ctx, (LiteralNode*)node);

        case NODE_BINARY_OP:
            return fold_binary_op(ctx, (BinaryOpNode*)node);

        case NODE_UNARY_OP:
            return fold_unary_op(ctx, (UnaryOpNode*)node);

        case NODE_MEMBER_ACCESS: {
            MemberAccessNode *ma = (MemberAccessNode*)node;
            VarType obj_t = node->sem_type;
            if (obj_t.base == TYPE_ENUM && obj_t.class_name && ma->member_name) {
                long val = 0;
                if (alir_get_enum_value(ctx->module, obj_t.class_name, ma->member_name, &val)) {
                    return alir_const_int(ctx->module, val);
                }
            }
            return NULL;
        }

        case NODE_VAR_REF: {
            VarRefNode *vr = (VarRefNode*)node;
            VarType t = node->sem_type;
            if (t.base == TYPE_ENUM && t.class_name) {
                long val = 0;
                if (alir_get_enum_value(ctx->module, t.class_name, vr->name, &val)) {
                    return alir_const_int(ctx->module, val);
                }
            }
            if (strcmp(vr->name, "Err") == 0) {
                void *err_val = hashmap_get(&ctx->sem->compiler_ctx->error_table, vr->name);
                if (err_val) {
                    return alir_const_int(ctx->module, (long)(intptr_t)err_val);
                }
            }
            AlirValue *val = hashmap_get(&ctx->const_fold_map, vr->name);
            if (!val) {
                val = hashmap_get(&ctx->module->const_fold_map, vr->name);
            }
            if (val) return val;
            return NULL;
        }

        case NODE_CAST: {
            CastNode *cn = (CastNode*)node;
            AlirValue *val = fold_const_expr_ahead(ctx, cn->operand);
            if (!val || val->kind != ALIR_VAL_CONST) return NULL;
            VarType target = cn->var_type;
            if (target.base == TYPE_UNKNOWN || target.base == TYPE_AUTO) return val;
            if (val->type.base == target.base && val->type.ptr_depth == target.ptr_depth) return val;
            if (val->type.base == TYPE_INT && target.base == TYPE_DOUBLE)
                return alir_const_double(ctx->module, (double)val->val.int_val);
            if (val->type.base == TYPE_INT && target.base == TYPE_SINGLE)
                return alir_const_float(ctx->module, (float)val->val.int_val);
            if (val->type.base == TYPE_DOUBLE && target.base == TYPE_INT)
                return alir_const_int(ctx->module, (long)val->val.double_val);
            if (val->type.base == TYPE_DOUBLE && target.base == TYPE_SINGLE)
                return alir_const_float(ctx->module, (float)val->val.double_val);
            if (val->type.base == TYPE_SINGLE && target.base == TYPE_INT)
                return alir_const_int(ctx->module, (long)val->val.single_val);
            if (val->type.base == TYPE_SINGLE && target.base == TYPE_DOUBLE)
                return alir_const_double(ctx->module, (double)val->val.single_val);
            return val;
        }

        default:
            return NULL;
    }
}

AlirValue* alir_fold_const_expr(AlirCtx *ctx, ASTNode *node, VarType target) {
    if (!node) return NULL;
    AlirValue *val = fold_const_expr_ahead(ctx, node);
    if (!val) return NULL;
    if (target.base == TYPE_UNKNOWN || target.base == TYPE_AUTO) return val;
    if (val->type.base == target.base && val->type.ptr_depth == target.ptr_depth) return val;
    if (val->type.base == TYPE_INT && target.base == TYPE_DOUBLE)
        return alir_const_double(ctx->module, (double)val->val.int_val);
    if (val->type.base == TYPE_INT && target.base == TYPE_SINGLE)
        return alir_const_float(ctx->module, (float)val->val.int_val);
    if (val->type.base == TYPE_DOUBLE && target.base == TYPE_INT)
        return alir_const_int(ctx->module, (long)val->val.double_val);
    if (val->type.base == TYPE_DOUBLE && target.base == TYPE_SINGLE)
        return alir_const_float(ctx->module, (float)val->val.double_val);
    if (val->type.base == TYPE_SINGLE && target.base == TYPE_INT)
        return alir_const_int(ctx->module, (long)val->val.single_val);
    if (val->type.base == TYPE_SINGLE && target.base == TYPE_DOUBLE)
        return alir_const_double(ctx->module, (double)val->val.single_val);
    return val;
}

void scan_and_fold_consts(AlirCtx *ctx, ASTNode *node) {
    while (node) {
        if (node->type == NODE_NAMESPACE) {
            scan_and_fold_consts(ctx, ((NamespaceNode*)node)->body);
        } else if (node->type == NODE_VAR_DECL) {
            VarDeclNode *vn = (VarDeclNode*)node;
            if (vn->is_const && vn->initializer) {
                AlirValue *folded = alir_fold_const_expr(ctx, vn->initializer, vn->var_type);
                if (folded) {
                    AlirConstFoldEntry *entry = alir_alloc(ctx->module, sizeof(AlirConstFoldEntry));
                    entry->name = alir_strdup(ctx->module, vn->name);
                    entry->value = folded;
                    entry->next = ctx->const_folds;
                    ctx->const_folds = entry;
                    AlirConstFoldEntry *mod_entry = alir_alloc(ctx->module, sizeof(AlirConstFoldEntry));
                    mod_entry->name = entry->name;
                    mod_entry->value = entry->value;
                    mod_entry->next = ctx->module->const_folds;
                    ctx->module->const_folds = mod_entry;
                    hashmap_put(&ctx->const_fold_map, entry->name, entry->value);
                    hashmap_put(&ctx->module->const_fold_map, entry->name, entry->value);
                }
            }
        }
        node = node->next;
    }
}

