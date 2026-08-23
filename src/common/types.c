#include "common/types.h"
#include "common/intern.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static TypeContext g_default_ctx;

/**
 * @brief A node in the canonical type hash table chain.
 */
typedef struct TypeNode {
    VarType type;
    struct TypeNode *next;
} TypeNode;

#define TYPE_HASHTABLE_SIZE 1024

/**
 * @brief Computes a hash key for a canonical type descriptor.
 */
static uint32_t hash_type_key(
    BaseType base, int ptr_depth, const char *class_name,
    int array_size, int array_depth, bool is_unsigned,
    bool is_tainted, bool is_pristine, bool is_func_ptr,
    VarType *fp_ret_type, VarType **fp_param_types,
    int fp_param_count, bool fp_is_varargs
) {
    uint32_t h = 0x811c9dc5;
    h = (h ^ (uint32_t)base) * 16777619;
    h = (h ^ (uint32_t)ptr_depth) * 16777619;
    h = (h ^ (uint32_t)(uintptr_t)class_name) * 16777619;
    h = (h ^ (uint32_t)array_size) * 16777619;
    h = (h ^ (uint32_t)array_depth) * 16777619;
    uint32_t flags = (is_unsigned ? 1 : 0) |
                     (is_tainted ? 2 : 0) |
                     (is_pristine ? 4 : 0) |
                     (is_func_ptr ? 8 : 0) |
                     (fp_is_varargs ? 16 : 0);
    h = (h ^ flags) * 16777619;
    h = (h ^ (uint32_t)(uintptr_t)fp_ret_type) * 16777619;
    h = (h ^ (uint32_t)fp_param_count) * 16777619;
    if (fp_param_types) {
        for (int i = 0; i < fp_param_count; i++) {
            h = (h ^ (uint32_t)(uintptr_t)fp_param_types[i]) * 16777619;
        }
    }
    return h;
}

/**
 * @brief Checks whether a VarType matches all the given field values.
 */
static bool type_fields_match(
    const VarType *t, BaseType base, int ptr_depth,
    const char *class_name, int array_size, int array_depth,
    bool is_unsigned, bool is_tainted, bool is_pristine,
    bool is_func_ptr, VarType *fp_ret_type,
    VarType **fp_param_types, int fp_param_count, bool fp_is_varargs
) {
    if (!t) return false;
    if (t->base != base || t->ptr_depth != ptr_depth || t->array_size != array_size || t->array_depth != array_depth) return false;
    if (t->class_name != class_name) return false;
    if (t->is_unsigned != is_unsigned || t->is_tainted != is_tainted || t->is_pristine != is_pristine) return false;
    if (t->is_func_ptr != is_func_ptr) return false;
    if (is_func_ptr) {
        if (t->fp_ret_type != fp_ret_type || t->fp_param_count != fp_param_count || t->fp_is_varargs != fp_is_varargs) return false;
        for (int i = 0; i < fp_param_count; i++) {
            if (t->fp_param_types[i] != fp_param_types[i]) return false;
        }
    }
    return true;
}

/**
 * @brief Internal: retrieves or creates a canonical type descriptor.
 */
static VarType* types_get_canonical_type_full_ctx(
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
) {
    if (!ctx->initialized) {
        types_context_init(ctx, NULL);
    }

    const char *interned_class_name = class_name ? intern_string(class_name) : NULL;

    uint32_t hash = hash_type_key(
        base, ptr_depth, interned_class_name, array_size, array_depth,
        is_unsigned, is_tainted, is_pristine, is_func_ptr,
        fp_ret_type, fp_param_types, fp_param_count, fp_is_varargs
    );

    uint32_t idx = hash % TYPE_HASHTABLE_SIZE;
    TypeNode *curr = ctx->table[idx];
    while (curr) {
        if (type_fields_match(
                &curr->type, base, ptr_depth, interned_class_name, array_size, array_depth,
                is_unsigned, is_tainted, is_pristine, is_func_ptr,
                fp_ret_type, fp_param_types, fp_param_count, fp_is_varargs
            )) {
            return &curr->type;
        }
        curr = curr->next;
    }

    TypeNode *node = (TypeNode*)arena_alloc(&ctx->arena, sizeof(TypeNode));
    memset(node, 0, sizeof(TypeNode));

    node->type.base = base;
    node->type.ptr_depth = ptr_depth;
    node->type.class_name = (char*)interned_class_name;
    node->type.array_size = array_size;
    node->type.array_depth = array_depth;
    node->type.is_unsigned = is_unsigned;
    node->type.is_tainted = is_tainted;
    node->type.is_pristine = is_pristine;
    node->type.is_func_ptr = is_func_ptr;

    if (is_func_ptr) {
        node->type.fp_ret_type = fp_ret_type;
        node->type.fp_param_count = fp_param_count;
        node->type.fp_is_varargs = fp_is_varargs;
        if (fp_param_count > 0 && fp_param_types) {
            VarType **param_copy = (VarType**)arena_alloc(&ctx->arena, fp_param_count * sizeof(VarType*));
            memcpy(param_copy, fp_param_types, fp_param_count * sizeof(VarType*));
            node->type.fp_param_types = param_copy;
        }
    }

    node->next = ctx->table[idx];
    ctx->table[idx] = node;

    return &node->type;
}

void types_context_init(TypeContext *ctx, Arena *arena) {
    if (ctx->initialized) return;
    ctx->table = (TypeNode**)calloc(TYPE_HASHTABLE_SIZE, sizeof(TypeNode*));
    if (arena) {
        ctx->arena = *arena;
    } else {
        arena_init(&ctx->arena);
    }
    ctx->initialized = true;
    memset(ctx->singletons, 0, sizeof(ctx->singletons));
}

void types_context_destroy(TypeContext *ctx) {
    if (!ctx->initialized) return;
    free(ctx->table);
    arena_free(&ctx->arena);
    ctx->initialized = false;
    memset(ctx->singletons, 0, sizeof(ctx->singletons));
}

TypeContext* types_get_default_context(void) {
    if (!g_default_ctx.initialized) {
        types_init();
    }
    return &g_default_ctx;
}

/**
 * @brief Initializes the default global type context and primitive singletons.
 */
void types_init(void) {
    types_context_init(&g_default_ctx, NULL);

    g_default_ctx.singletons[TYPE_VOID] = types_get_canonical_type(&g_default_ctx, TYPE_VOID, 0, NULL);
    g_default_ctx.singletons[TYPE_INT] = types_get_canonical_type(&g_default_ctx, TYPE_INT, 0, NULL);
    g_default_ctx.singletons[TYPE_UNSIGNED_INT] = types_get_canonical_type_full(&g_default_ctx, TYPE_UNSIGNED_INT, 0, NULL, 0, 0, true, false, false, false, NULL, NULL, 0, false);
    g_default_ctx.singletons[TYPE_SHORT] = types_get_canonical_type(&g_default_ctx, TYPE_SHORT, 0, NULL);
    g_default_ctx.singletons[TYPE_LONG] = types_get_canonical_type(&g_default_ctx, TYPE_LONG, 0, NULL);
    g_default_ctx.singletons[TYPE_LONG_LONG] = types_get_canonical_type(&g_default_ctx, TYPE_LONG_LONG, 0, NULL);
    g_default_ctx.singletons[TYPE_UNSIGNED_LONG] = types_get_canonical_type_full(&g_default_ctx, TYPE_UNSIGNED_LONG, 0, NULL, 0, 0, true, false, false, false, NULL, NULL, 0, false);
    g_default_ctx.singletons[TYPE_UNSIGNED_LONG_LONG] = types_get_canonical_type_full(&g_default_ctx, TYPE_UNSIGNED_LONG_LONG, 0, NULL, 0, 0, true, false, false, false, NULL, NULL, 0, false);
    g_default_ctx.singletons[TYPE_CHAR] = types_get_canonical_type(&g_default_ctx, TYPE_CHAR, 0, NULL);
    g_default_ctx.singletons[TYPE_UNSIGNED_CHAR] = types_get_canonical_type_full(&g_default_ctx, TYPE_UNSIGNED_CHAR, 0, NULL, 0, 0, true, false, false, false, NULL, NULL, 0, false);
    g_default_ctx.singletons[TYPE_BOOL] = types_get_canonical_type(&g_default_ctx, TYPE_BOOL, 0, NULL);
    g_default_ctx.singletons[TYPE_SINGLE] = types_get_canonical_type(&g_default_ctx, TYPE_SINGLE, 0, NULL);
    g_default_ctx.singletons[TYPE_DOUBLE] = types_get_canonical_type(&g_default_ctx, TYPE_DOUBLE, 0, NULL);
    g_default_ctx.singletons[TYPE_LONG_DOUBLE] = types_get_canonical_type(&g_default_ctx, TYPE_LONG_DOUBLE, 0, NULL);
    g_default_ctx.singletons[TYPE_AUTO] = types_get_canonical_type(&g_default_ctx, TYPE_AUTO, 0, NULL);
    g_default_ctx.singletons[TYPE_CLASS] = types_get_canonical_type(&g_default_ctx, TYPE_CLASS, 0, NULL);
    g_default_ctx.singletons[TYPE_ENUM] = types_get_canonical_type(&g_default_ctx, TYPE_ENUM, 0, NULL);
    g_default_ctx.singletons[TYPE_NAMESPACE] = types_get_canonical_type(&g_default_ctx, TYPE_NAMESPACE, 0, NULL);
    g_default_ctx.singletons[TYPE_ERROR] = types_get_canonical_type(&g_default_ctx, TYPE_ERROR, 0, NULL);
    g_default_ctx.singletons[TYPE_UNKNOWN] = types_get_canonical_type(&g_default_ctx, TYPE_UNKNOWN, 0, NULL);
}

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
) {
    return types_get_canonical_type_full_ctx(ctx, base, ptr_depth, class_name,
        array_size, array_depth, is_unsigned, is_tainted, is_pristine,
        is_func_ptr, fp_ret_type, fp_param_types, fp_param_count, fp_is_varargs);
}

VarType* types_get_canonical_type(TypeContext *ctx, BaseType base, int ptr_depth, const char *class_name) {
    return types_get_canonical_type_full(ctx, base, ptr_depth, class_name, 0, 0, false, false, false, false, NULL, NULL, 0, false);
}

VarType* types_get_canonical_array_type(TypeContext *ctx, VarType *element_type, int size) {
    if (!element_type) return types_get_canonical_type(ctx, TYPE_UNKNOWN, 0, NULL);
    return types_get_canonical_type_full(ctx,
        element_type->base, element_type->ptr_depth, element_type->class_name,
        size, element_type->array_depth + 1,
        element_type->is_unsigned, element_type->is_tainted, element_type->is_pristine,
        element_type->is_func_ptr, element_type->fp_ret_type, element_type->fp_param_types,
        element_type->fp_param_count, element_type->fp_is_varargs
    );
}

VarType* types_get_canonical_ptr_type(TypeContext *ctx, VarType *base_type) {
    if (!base_type) return types_get_canonical_type(ctx, TYPE_UNKNOWN, 0, NULL);
    return types_get_canonical_type_full(ctx,
        base_type->base, base_type->ptr_depth + 1, base_type->class_name,
        base_type->array_size, base_type->array_depth,
        base_type->is_unsigned, base_type->is_tainted, base_type->is_pristine,
        base_type->is_func_ptr, base_type->fp_ret_type, base_type->fp_param_types,
        base_type->fp_param_count, base_type->fp_is_varargs
    );
}

VarType* types_get_canonical_func_ptr_type(TypeContext *ctx, VarType *ret_type, VarType **param_types, int param_count, bool is_varargs) {
    return types_get_canonical_type_full(ctx,
        TYPE_VOID, 0, NULL, 0, 0, false, false, false,
        true, ret_type, param_types, param_count, is_varargs
    );
}

bool types_are_equal(const VarType* a, const VarType* b) {
    return a == b;
}

// Backward-compatible global wrappers

void types_init_global(void) {
    types_init();
}

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
) {
    return types_get_canonical_type_full(&g_default_ctx, base, ptr_depth, class_name,
        array_size, array_depth, is_unsigned, is_tainted, is_pristine,
        is_func_ptr, fp_ret_type, fp_param_types, fp_param_count, fp_is_varargs);
}

VarType* get_canonical_type(BaseType base, int ptr_depth, const char *class_name) {
    return types_get_canonical_type(&g_default_ctx, base, ptr_depth, class_name);
}

VarType* get_canonical_array_type(VarType *element_type, int size) {
    return types_get_canonical_array_type(&g_default_ctx, element_type, size);
}

VarType* get_canonical_ptr_type(VarType *base_type) {
    return types_get_canonical_ptr_type(&g_default_ctx, base_type);
}

VarType* get_canonical_func_ptr_type(VarType *ret_type, VarType **param_types, int param_count, bool is_varargs) {
    return types_get_canonical_func_ptr_type(&g_default_ctx, ret_type, param_types, param_count, is_varargs);
}
