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

AlirValue* alir_const_float(AlirModule *mod, float val) {
    AlirValue *v = alir_alloc(mod, sizeof(AlirValue));
    v->kind = ALIR_VAL_CONST;
    v->type = (VarType){TYPE_FLOAT, 0};
    v->val.float_val = val;
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
    v->type = (VarType){TYPE_CLASS, 0, 0, alir_strdup(mod, type_name), 0, 0};
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
