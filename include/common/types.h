#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include "parser/typestruct.h"
#include "common/arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef BaseType VarTypeKind;

/**
 * @brief Opaque handle to a per-compilation type context.
 *
 * All canonical type state (hash table, arena, primitive singletons) lives
 * inside a TypeContext so the type system is no longer process-global.
 */
typedef struct TypeContext {
    TypeNode **table;
    Arena arena;
    bool initialized;
    VarType *singletons[TYPE_UNKNOWN + 1];
} TypeContext;

/**
 * @brief Initializes a type context with an optional pre-configured arena.
 * @param ctx The type context to initialize.
 * @param arena If non-NULL, use this arena; otherwise allocate an internal one.
 */
void types_context_init(TypeContext *ctx, Arena *arena);

/**
 * @brief Destroys a type context, freeing its hash table and arena.
 * @param ctx The type context to destroy.
 */
void types_context_destroy(TypeContext *ctx);

/**
 * @brief Canonical type constructor with full specification.
 * @param ctx The type context.
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
VarType* types_get_canonical_type_full(
    TypeContext *ctx,
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
 * @param ctx The type context.
 * @param base The base type kind.
 * @param ptr_depth Pointer indirection depth.
 * @param class_name Class name for class types, or NULL.
 * @return A pointer to the canonical VarType.
 */
VarType* types_get_canonical_type(TypeContext *ctx, BaseType base, int ptr_depth, const char *class_name);

/**
 * @brief Retrieves or creates a canonical array type.
 * @param ctx The type context.
 * @param element_type The element type of the array.
 * @param size The array size.
 * @return A pointer to the canonical array VarType.
 */
VarType* types_get_canonical_array_type(TypeContext *ctx, VarType *element_type, int size);

/**
 * @brief Retrieves or creates a canonical pointer type.
 * @param ctx The type context.
 * @param base_type The pointed-to type.
 * @return A pointer to the canonical pointer VarType.
 */
VarType* types_get_canonical_ptr_type(TypeContext *ctx, VarType *base_type);

/**
 * @brief Retrieves or creates a canonical function-pointer type.
 * @param ctx The type context.
 * @param ret_type The return type of the function.
 * @param param_types Array of parameter types.
 * @param param_count Number of parameters.
 * @param is_varargs Whether the function is variadic.
 * @return A pointer to the canonical function-pointer VarType.
 */
VarType* types_get_canonical_func_ptr_type(TypeContext *ctx, VarType *ret_type, VarType **param_types, int param_count, bool is_varargs);

/**
 * @brief Checks structural equality of two types via pointer identity.
 * @param a First type.
 * @param b Second type.
 * @return true if a and b point to the same canonical type.
 */
bool types_are_equal(const VarType* a, const VarType* b);

/**
 * @brief Returns the default global type context (lazy-initialized).
 * @return Pointer to the default type context.
 */
TypeContext* types_get_default_context(void);

#ifdef __cplusplus
}
#endif

#endif // TYPES_H
