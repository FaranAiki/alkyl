/**
 * @file const.h
 * @brief ALIR constant value constructors.
 */
#ifndef ALIR_CONST_H
#define ALIR_CONST_H

#include "alir.h"

/**
 * @brief Creates a constant integer ALIR value.
 * @param mod The ALIR module.
 * @param val The integer value.
 * @return The created ALIR value.
 */
AlirValue* alir_const_int(AlirModule *mod, long val);

/**
 * @brief Creates a constant char ALIR value.
 * @param mod The ALIR module.
 * @param val The char value.
 * @return The created ALIR value.
 */
AlirValue* alir_const_char(AlirModule *mod, char val);

/**
 * @brief Creates a constant unsigned char ALIR value.
 * @param mod The ALIR module.
 * @param val The unsigned char value.
 * @return The created ALIR value.
 */
AlirValue* alir_const_unsigned_char(AlirModule *mod, unsigned char val);

/**
 * @brief Creates a constant unsigned int ALIR value.
 * @param mod The ALIR module.
 * @param val The unsigned int value.
 * @return The created ALIR value.
 */
AlirValue* alir_const_unsigned_int(AlirModule *mod, unsigned int val);

/**
 * @brief Creates a constant float ALIR value.
 * @param mod The ALIR module.
 * @param val The float value.
 * @return The created ALIR value.
 */
AlirValue* alir_const_float(AlirModule *mod, float val);

/**
 * @brief Creates a constant double ALIR value.
 * @param mod The ALIR module.
 * @param val The double value.
 * @return The created ALIR value.
 */
AlirValue* alir_const_double(AlirModule *mod, double val);

/**
 * @brief Creates a constant long ALIR value.
 * @param mod The ALIR module.
 * @param val The long value.
 * @return The created ALIR value.
 */
AlirValue* alir_const_long(AlirModule *mod, long val);

/**
 * @brief Creates a constant long long ALIR value.
 * @param mod The ALIR module.
 * @param val The long long value.
 * @return The created ALIR value.
 */
AlirValue* alir_const_long_long(AlirModule *mod, long long val);

/**
 * @brief Creates a constant unsigned long ALIR value.
 * @param mod The ALIR module.
 * @param val The unsigned long value.
 * @return The created ALIR value.
 */
AlirValue* alir_const_unsigned_long(AlirModule *mod, unsigned long val);

/**
 * @brief Creates a constant unsigned long long ALIR value.
 * @param mod The ALIR module.
 * @param val The unsigned long long value.
 * @return The created ALIR value.
 */
AlirValue* alir_const_unsigned_long_long(AlirModule *mod, unsigned long long val);

/**
 * @brief Creates a temporary ALIR value.
 * @param mod The ALIR module.
 * @param t The type of the temporary.
 * @param id The temporary ID.
 * @return The created ALIR value.
 */
AlirValue* alir_val_temp(AlirModule *mod, VarType t, int id);

/**
 * @brief Creates a variable ALIR value.
 * @param mod The ALIR module.
 * @param name The variable name.
 * @return The created ALIR value.
 */
AlirValue* alir_val_var(AlirModule *mod, const char *name);

/**
 * @brief Creates a global ALIR value.
 * @param mod The ALIR module.
 * @param name The global name.
 * @param type The type of the global.
 * @return The created ALIR value.
 */
AlirValue* alir_val_global(AlirModule *mod, const char *name, VarType type);

/**
 * @brief Creates a label ALIR value.
 * @param mod The ALIR module.
 * @param label The label name.
 * @return The created ALIR value.
 */
AlirValue* alir_val_label(AlirModule *mod, const char *label);

/**
 * @brief Creates a type ALIR value.
 * @param mod The ALIR module.
 * @param type_name The type name.
 * @return The created ALIR value.
 */
AlirValue* alir_val_type(AlirModule *mod, const char *type_name);

#endif // ALIR_CONST_H
