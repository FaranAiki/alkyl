#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include "parser/typestruct.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef BaseType VarTypeKind;

// Pre-allocated primitive type singletons
extern VarType *g_type_void;
extern VarType *g_type_int;
extern VarType *g_type_unsigned_int;
extern VarType *g_type_short;
extern VarType *g_type_long;
extern VarType *g_type_long_long;
extern VarType *g_type_unsigned_long;
extern VarType *g_type_unsigned_long_long;
extern VarType *g_type_char;
extern VarType *g_type_unsigned_char;
extern VarType *g_type_bool;
extern VarType *g_type_single;
extern VarType *g_type_double;
extern VarType *g_type_long_double;
extern VarType *g_type_auto;
extern VarType *g_type_class;
extern VarType *g_type_enum;
extern VarType *g_type_namespace;
extern VarType *g_type_error;
extern VarType *g_type_unknown;

// Initialize global type singletons and canonical map
void types_init(void);

// Canonical type constructors
VarType* get_canonical_type_full(
    BaseType base,
    int ptr_depth,
    const char *class_name,
    int array_size,
    int array_depth,
    bool is_unsigned,
    bool is_tainted,
    bool is_pristine,
    bool is_func_ptr,
    VarType *fp_ret_type,
    VarType **fp_param_types,
    int fp_param_count,
    bool fp_is_varargs
);

VarType* get_canonical_type(BaseType base, int ptr_depth, const char *class_name);
VarType* get_canonical_array_type(VarType *element_type, int size);
VarType* get_canonical_ptr_type(VarType *base_type);
VarType* get_canonical_func_ptr_type(VarType *ret_type, VarType **param_types, int param_count, bool is_varargs);

// Structural equality evaluating pointer equality (a == b)
bool types_are_equal(const VarType* a, const VarType* b);
bool sem_types_are_equal(const VarType* a, const VarType* b);

#ifdef __cplusplus
}
#endif

#endif // TYPES_H
