#include "common/types.h"
#include "common/intern.h"
#include "common/arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

VarType *g_type_void = NULL;
VarType *g_type_int = NULL;
VarType *g_type_unsigned_int = NULL;
VarType *g_type_short = NULL;
VarType *g_type_long = NULL;
VarType *g_type_long_long = NULL;
VarType *g_type_unsigned_long = NULL;
VarType *g_type_unsigned_long_long = NULL;
VarType *g_type_char = NULL;
VarType *g_type_unsigned_char = NULL;
VarType *g_type_bool = NULL;
VarType *g_type_single = NULL;
VarType *g_type_double = NULL;
VarType *g_type_long_double = NULL;
VarType *g_type_auto = NULL;
VarType *g_type_class = NULL;
VarType *g_type_enum = NULL;
VarType *g_type_namespace = NULL;
VarType *g_type_error = NULL;
VarType *g_type_unknown = NULL;

typedef struct TypeNode {
    VarType type;
    struct TypeNode *next;
} TypeNode;

#define TYPE_HASHTABLE_SIZE 1024
static TypeNode *g_type_table[TYPE_HASHTABLE_SIZE];
static Arena g_types_arena;
static bool g_types_inited = false;

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
    if (!g_types_inited) {
        types_init();
    }

    const char *interned_class_name = class_name ? intern_string(class_name) : NULL;

    uint32_t hash = hash_type_key(
        base, ptr_depth, interned_class_name, array_size, array_depth,
        is_unsigned, is_tainted, is_pristine, is_func_ptr,
        fp_ret_type, fp_param_types, fp_param_count, fp_is_varargs
    );

    uint32_t idx = hash % TYPE_HASHTABLE_SIZE;
    TypeNode *curr = g_type_table[idx];
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

    TypeNode *node = (TypeNode*)arena_alloc(&g_types_arena, sizeof(TypeNode));
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
            VarType **param_copy = (VarType**)arena_alloc(&g_types_arena, fp_param_count * sizeof(VarType*));
            memcpy(param_copy, fp_param_types, fp_param_count * sizeof(VarType*));
            node->type.fp_param_types = param_copy;
        }
    }

    node->next = g_type_table[idx];
    g_type_table[idx] = node;

    return &node->type;
}

VarType* get_canonical_type(BaseType base, int ptr_depth, const char *class_name) {
    return get_canonical_type_full(base, ptr_depth, class_name, 0, 0, false, false, false, false, NULL, NULL, 0, false);
}

VarType* get_canonical_array_type(VarType *element_type, int size) {
    if (!element_type) return g_type_unknown;
    return get_canonical_type_full(
        element_type->base, element_type->ptr_depth, element_type->class_name,
        size, element_type->array_depth + 1,
        element_type->is_unsigned, element_type->is_tainted, element_type->is_pristine,
        element_type->is_func_ptr, element_type->fp_ret_type, element_type->fp_param_types,
        element_type->fp_param_count, element_type->fp_is_varargs
    );
}

VarType* get_canonical_ptr_type(VarType *base_type) {
    if (!base_type) return g_type_unknown;
    return get_canonical_type_full(
        base_type->base, base_type->ptr_depth + 1, base_type->class_name,
        base_type->array_size, base_type->array_depth,
        base_type->is_unsigned, base_type->is_tainted, base_type->is_pristine,
        base_type->is_func_ptr, base_type->fp_ret_type, base_type->fp_param_types,
        base_type->fp_param_count, base_type->fp_is_varargs
    );
}

VarType* get_canonical_func_ptr_type(VarType *ret_type, VarType **param_types, int param_count, bool is_varargs) {
    return get_canonical_type_full(
        TYPE_VOID, 0, NULL, 0, 0, false, false, false,
        true, ret_type, param_types, param_count, is_varargs
    );
}

void types_init(void) {
    if (g_types_inited) return;
    arena_init(&g_types_arena, 128 * 1024);
    memset(g_type_table, 0, sizeof(g_type_table));
    g_types_inited = true;

    g_type_void = get_canonical_type(TYPE_VOID, 0, NULL);
    g_type_int = get_canonical_type(TYPE_INT, 0, NULL);
    g_type_unsigned_int = get_canonical_type_full(TYPE_UNSIGNED_INT, 0, NULL, 0, 0, true, false, false, false, NULL, NULL, 0, false);
    g_type_short = get_canonical_type(TYPE_SHORT, 0, NULL);
    g_type_long = get_canonical_type(TYPE_LONG, 0, NULL);
    g_type_long_long = get_canonical_type(TYPE_LONG_LONG, 0, NULL);
    g_type_unsigned_long = get_canonical_type_full(TYPE_UNSIGNED_LONG, 0, NULL, 0, 0, true, false, false, false, NULL, NULL, 0, false);
    g_type_unsigned_long_long = get_canonical_type_full(TYPE_UNSIGNED_LONG_LONG, 0, NULL, 0, 0, true, false, false, false, NULL, NULL, 0, false);
    g_type_char = get_canonical_type(TYPE_CHAR, 0, NULL);
    g_type_unsigned_char = get_canonical_type_full(TYPE_UNSIGNED_CHAR, 0, NULL, 0, 0, true, false, false, false, NULL, NULL, 0, false);
    g_type_bool = get_canonical_type(TYPE_BOOL, 0, NULL);
    g_type_single = get_canonical_type(TYPE_SINGLE, 0, NULL);
    g_type_double = get_canonical_type(TYPE_DOUBLE, 0, NULL);
    g_type_long_double = get_canonical_type(TYPE_LONG_DOUBLE, 0, NULL);
    g_type_auto = get_canonical_type(TYPE_AUTO, 0, NULL);
    g_type_class = get_canonical_type(TYPE_CLASS, 0, NULL);
    g_type_enum = get_canonical_type(TYPE_ENUM, 0, NULL);
    g_type_namespace = get_canonical_type(TYPE_NAMESPACE, 0, NULL);
    g_type_error = get_canonical_type(TYPE_ERROR, 0, NULL);
    g_type_unknown = get_canonical_type(TYPE_UNKNOWN, 0, NULL);
}

bool types_are_equal(const VarType* a, const VarType* b) {
    return a == b;
}

bool sem_types_are_equal(const VarType* a, const VarType* b) {
    return a == b;
}
