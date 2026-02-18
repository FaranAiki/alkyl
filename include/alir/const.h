#ifndef ALIR_CONST_H
#define ALIR_CONST_H

#include "alir.h"

AlirValue* alir_const_int(AlirModule *mod, long val);
AlirValue* alir_const_float(AlirModule *mod, double val);
AlirValue* alir_val_temp(AlirModule *mod, VarType t, int id);
AlirValue* alir_val_var(AlirModule *mod, const char *name);
AlirValue* alir_val_global(AlirModule *mod, const char *name, VarType type);
AlirValue* alir_val_label(AlirModule *mod, const char *label);
AlirValue* alir_val_type(AlirModule *mod, const char *type_name);

#endif // ALIR_CONST_H
