#ifndef ALIR_CONST_H
#define ALIR_CONST_H

#include "alir.h"

AlirValue* alir_const_int(long val);
AlirValue* alir_const_float(double val);
AlirValue* alir_val_temp(VarType t, int id);
AlirValue* alir_val_var(const char *name);
AlirValue* alir_val_global(const char *name, VarType type);
AlirValue* alir_val_label(const char *label);
AlirValue* alir_val_type(const char *type_name);
void alir_register_enum(AlirModule *mod, const char *name, AlirEnumEntry *entries);
AlirEnum* alir_find_enum(AlirModule *mod, const char *name);
int alir_get_enum_value(AlirModule *mod, const char *enum_name, const char *entry_name, long *out_val);

#endif // ALIR_CONST_H
