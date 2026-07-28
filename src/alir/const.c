#include "alir.h"

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
    v->val.unsigned_long_val = val; // Assuming union has unsigned_long_val for both, or should I use unsigned_long_val for unsigned long long? Wait, the union in aliases.h has unsigned_long_val which is unsigned long long.
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

void alir_register_enum(AlirModule *mod, const char *name, AlirEnumEntry *entries) {
    AlirEnum *e = alir_alloc(mod, sizeof(AlirEnum));
    e->name = alir_strdup(mod, name);
    e->entries = entries;
    e->next = mod->enums;
    mod->enums = e;
}

AlirEnum* alir_find_enum(AlirModule *mod, const char *name) {
    AlirEnum *curr = mod->enums;
    while(curr) {
        if (strcmp(curr->name, name) == 0) return curr;
        curr = curr->next;
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

AlirValue* alir_fold_const_expr(AlirCtx *ctx, ASTNode *node, VarType target) {
    if (!node) return NULL;
    AlirValue *val = alir_gen_expr(ctx, node);
    if (!val || val->kind != ALIR_VAL_CONST) return NULL;
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
                }
            }
        }
        node = node->next;
    }
}

