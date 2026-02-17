#include "alir.h"

AlirValue* alir_const_int(long val) {
    AlirValue *v = calloc(1, sizeof(AlirValue));
    v->kind = ALIR_VAL_CONST;
    v->type = (VarType){TYPE_INT, 0, NULL, 0, 0};
    v->int_val = val;
    return v;
}

AlirValue* alir_const_float(double val) {
    AlirValue *v = calloc(1, sizeof(AlirValue));
    v->kind = ALIR_VAL_CONST;
    v->type = (VarType){TYPE_DOUBLE, 0, NULL, 0, 0};
    v->float_val = val;
    return v;
}

AlirValue* alir_val_temp(VarType t, int id) {
    AlirValue *v = calloc(1, sizeof(AlirValue));
    v->kind = ALIR_VAL_TEMP;
    v->type = t;
    v->temp_id = id;
    return v;
}

AlirValue* alir_val_var(const char *name) {
    AlirValue *v = calloc(1, sizeof(AlirValue));
    v->kind = ALIR_VAL_VAR;
    v->str_val = strdup(name);
    return v;
}

AlirValue* alir_val_global(const char *name, VarType type) {
    AlirValue *v = calloc(1, sizeof(AlirValue));
    v->kind = ALIR_VAL_GLOBAL;
    v->str_val = strdup(name);
    v->type = type;
    return v;
}

AlirValue* alir_val_label(const char *label) {
    AlirValue *v = calloc(1, sizeof(AlirValue));
    v->kind = ALIR_VAL_LABEL;
    v->str_val = strdup(label);
    return v;
}

AlirValue* alir_val_type(const char *type_name) {
    AlirValue *v = calloc(1, sizeof(AlirValue));
    v->kind = ALIR_VAL_TYPE;
    v->str_val = strdup(type_name);
    v->type = (VarType){TYPE_CLASS, 0, strdup(type_name), 0, 0};
    return v;
}

void alir_register_enum(AlirModule *mod, const char *name, AlirEnumEntry *entries) {
    AlirEnum *e = calloc(1, sizeof(AlirEnum));
    e->name = strdup(name);
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
