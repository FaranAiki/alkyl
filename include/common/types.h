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

/**
 * @brief Initialize global type singletons and canonical map.
 */
void types_init(void);

/**
 * @brief Canonical type constructor with full specification.
 * @param base The base type kind.
 * @param ptr_depth Pointer indirection depth.
 * @param class_name Class name for class types, or NULL.
 * @param array_size Array size, or 0.
 * @param array_depth Array nesting depth.
 * @param is_unsigned Whether the type is unsigned.
 * @param is_tainted Whether the type is tainted.
 * @param is_pristine Whether the type is pristine.
 * @param is_func_ptr Whether the type is a function pointer.
 * @param fp_ret_type Return type of the function pointer, or NULL.
 * @param fp_param_types Parameter types of the function pointer, or NULL.
 * @param fp_param_count Number of parameters.
 * @param fp_is_varargs Whether the function pointer is variadic.
 * @return A pointer to the canonical VarType.
 */
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

/**
 * @brief Retrieves or creates a canonical type by base kind, pointer depth, and class name.
 * @param base The base type kind.
 * @param ptr_depth Pointer indirection depth.
 * @param class_name Class name for class types, or NULL.
 * @return A pointer to the canonical VarType.
 */
VarType* get_canonical_type(BaseType base, int ptr_depth, const char *class_name);
/**
 * @brief Retrieves or creates a canonical array type.
 * @param element_type The element type of the array.
 * @param size The array size.
 * @return A pointer to the canonical array VarType.
 */
VarType* get_canonical_array_type(VarType *element_type, int size);
/**
 * @brief Retrieves or creates a canonical pointer type.
 * @param base_type The pointed-to type.
 * @return A pointer to the canonical pointer VarType.
 */
VarType* get_canonical_ptr_type(VarType *base_type);
/**
 * @brief Retrieves or creates a canonical function-pointer type.
 * @param ret_type The return type of the function.
 * @param param_types Array of parameter types.
 * @param param_count Number of parameters.
 * @param is_varargs Whether the function is variadic.
 * @return A pointer to the canonical function-pointer VarType.
 */
VarType* get_canonical_func_ptr_type(VarType *ret_type, VarType **param_types, int param_count, bool is_varargs);

/**
 * @brief Checks structural equality of two types via pointer identity.
 * @param a First type.
 * @param b Second type.
 * @return true if a and b point to the same canonical type.
 */
bool types_are_equal(const VarType* a, const VarType* b);
/**
 * @brief Checks semantic equality of two types via pointer identity.
 * @param a First type.
 * @param b Second type.
 * @return true if a and b point to the same canonical type.
 */
bool sem_types_are_equal(const VarType* a, const VarType* b);

#ifdef __cplusplus
}
#endif

#endif // TYPES_H
