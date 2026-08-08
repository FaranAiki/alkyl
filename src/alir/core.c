#include "alir.h"
#include "../common/hashmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void* alir_alloc(AlirModule *mod, size_t size) {
    if (mod && mod->compiler_ctx && mod->compiler_ctx->arena) {
        void *ptr = arena_alloc(mod->compiler_ctx->arena, size);
        if (ptr) memset(ptr, 0, size);
        return ptr;
    }
    void *ptr = calloc(1, size);
    return ptr;
}

char* alir_strdup(AlirModule *mod, const char *str) {
    if (!str) return NULL;
    if (mod && mod->compiler_ctx) {
        return (char*)context_intern(mod->compiler_ctx, str);
    }
    return strdup(str);
}

AlirModule* alir_create_module(CompilerContext *ctx, const char *name) {
    AlirModule *m;
    if (ctx && ctx->arena) {
        m = arena_alloc_type(ctx->arena, AlirModule);
        memset(m, 0, sizeof(AlirModule));
    } else {
        m = calloc(1, sizeof(AlirModule));
    }

    m->compiler_ctx = ctx;
    m->name = alir_strdup(m, name);
    hashmap_init(&m->struct_map, ctx ? ctx->arena : NULL, 256);
    hashmap_init(&m->enum_map, ctx ? ctx->arena : NULL, 64);
    hashmap_init(&m->func_map, ctx ? ctx->arena : NULL, 64);
    hashmap_init(&m->const_fold_map, ctx ? ctx->arena : NULL, 64);
    return m;
}

 AlirFunction* alir_add_function(AlirModule *mod, const char *name, VarType ret, int is_flux) {
     debug_alir("alir_add_function: %s\n", name);
     AlirFunction *existing = hashmap_get(&mod->func_map, name);
     if (existing) {
         existing->ret_type = ret;
         existing->is_flux = is_flux;
         existing->blocks = NULL;
         existing->block_count = 0;
         existing->params = NULL;
         existing->param_count = 0;
         return existing;
     }

     AlirFunction *f = alir_alloc(mod, sizeof(AlirFunction));
     f->name = alir_strdup(mod, name);
     f->ret_type = ret;
     f->block_count = 0;
     f->is_flux = is_flux;
     f->is_varargs = 0;
     f->is_extern = 0;
     f->cconv = NULL;

     if (!mod->functions) {
         mod->functions = f;
     } else {
         AlirFunction *curr = mod->functions;
         while(curr->next) curr = curr->next;
         curr->next = f;
     }
     hashmap_put(&mod->func_map, name, f);
     return f;
 }

void alir_func_add_param(AlirModule *mod, AlirFunction *func, const char *name, VarType type) {
    AlirParam *p = alir_alloc(mod, sizeof(AlirParam));
    p->name = alir_strdup(mod, name ? name : "");
    p->type = type;

    if (!func->params) {
        func->params = p;
    } else {
        AlirParam *curr = func->params;
        while(curr->next) curr = curr->next;
        curr->next = p;
    }
    func->param_count++;
}

// Update alir_module_add_string_literal (around line 52) to use the passed VarType
AlirValue* alir_module_add_string_literal(AlirModule *mod, const char *content, VarType type) {
    AlirGlobal *curr = mod->globals;
    while (curr) {
        if (curr->string_content && streq_lit(curr->string_content, content)) {
            return alir_val_global(mod, curr->name, curr->type);
        }
        curr = curr->next;
    }
    char label[64];
    snprintf(label, sizeof(label), "str.%d", mod->str_counter++);

    AlirGlobal *g = alir_alloc(mod, sizeof(AlirGlobal));
    g->name = alir_strdup(mod, label);
    g->string_content = alir_strdup(mod, content);

    g->type = type;

    g->next = mod->globals;
    mod->globals = g;

    return alir_val_global(mod, label, g->type);
}

// Track labels locally per function to prevent collisions (e.g., "while_cond", "while_cond2")
// TODO change this
static HashMap label_map;
static AlirFunction *current_tracked_func = NULL;
static Arena *current_tracked_arena = NULL;

AlirBlock* alir_add_block(AlirModule *mod, AlirFunction *func, const char *label_hint) {
    Arena *arena = (mod && mod->compiler_ctx) ? mod->compiler_ctx->arena : NULL;

    if (func != current_tracked_func || arena != current_tracked_arena) {
        if (current_tracked_func && !current_tracked_arena) {
            hashmap_free(&label_map);
        }
        hashmap_init(&label_map, arena, 64);
        current_tracked_func = func;
        current_tracked_arena = arena;
    }

    AlirBlock *b = alir_alloc(mod, sizeof(AlirBlock));
    b->id = func->block_count; // Use block count as ID

    if (!label_hint) {
        char buf[32];
        snprintf(buf, sizeof(buf), "L%d", b->id);
        b->label = alir_strdup(mod, buf);
    } else {
        int count = hashmap_inc(&label_map, label_hint);
        if (count == 1) {
            b->label = alir_strdup(mod, label_hint); // First use gets the plain label
        } else {
            char buf[128];
            snprintf(buf, sizeof(buf), "%s_%d", label_hint, count); // Subsequents get enumerated (while_cond_2, etc)
            b->label = alir_strdup(mod, buf);
        }
    }

    if (!func->blocks) {
        func->blocks = b;
    } else {
        AlirBlock *curr = func->blocks;
        while(curr->next) curr = curr->next;
        curr->next = b;
    }
    func->block_count++;
    return b;
}

void alir_append_inst(AlirBlock *block, AlirInst *inst) {
    if (!block->head) {
        block->head = inst;
        block->tail = inst;
    } else {
        block->tail->next = inst;
        block->tail = inst;
    }
}

void alir_register_struct(AlirModule *mod, const char *name, AlirField *fields, int is_union) {
    AlirStruct *st = alir_alloc(mod, sizeof(AlirStruct));
    debug_alir("DEBUG_REGISTER: st=%p name=%s next=%p\n", st, name, mod->structs);
    fflush(stdout);
    st->is_union = is_union;
    st->name = alir_strdup(mod, name);
    st->fields = fields;

    if (fields == NULL) {
        st->field_count = -1; // Unresolved marker
    } else {
        st->field_count = 0;
        AlirField *f = fields;
        while(f) {
            st->field_count++;
            f = f->next;
        }
    }

    st->next = mod->structs;
    mod->structs = st;
    hashmap_put(&mod->struct_map, name, st);
}

AlirStruct* alir_find_struct(AlirModule *mod, const char *name) {
    AlirStruct *st = hashmap_get(&mod->struct_map, name);
    if (st) return st;
    st = mod->structs;
    while(st) {
        if (streq_lit(st->name, name)) {
            hashmap_put(&mod->struct_map, name, st);
            return st;
        }
        st = st->next;
    }
    // Fallback: Check if the struct name ends with ".name"
    st = mod->structs;
    while(st) {
        const char *dot = strrchr(st->name, '.');
        if (dot && streq_lit(dot + 1, name)) {
            return st;
        }
        st = st->next;
    }
    return NULL;
}

int alir_get_field_index(AlirModule *mod, const char *struct_name, const char *field_name) {
    AlirStruct *st = alir_find_struct(mod, struct_name);
    if (!st) return -1;

    AlirField *f = st->fields;
    while(f) {
        if (streq_lit(f->name, field_name)) return f->index;
        f = f->next;
    }
    return -1;
}

// TODO change this into ENUM!
const char* alir_op_str(AlirOpcode op) {
    switch(op) {
        case ALIR_OP_ALLOCA: return "alloc";
        case ALIR_OP_FREE_STACK: return "unstack"; // this is not needed, i guess
        case ALIR_OP_STORE: return "store";
        case ALIR_OP_LOAD: return "load";
        case ALIR_OP_GET_PTR: return "getptr";
        case ALIR_OP_BITCAST: return "bitcast";

        case ALIR_OP_ADD: return "add";
        case ALIR_OP_SUB: return "sub";
        case ALIR_OP_MUL: return "mul";
        case ALIR_OP_DIV: return "div";
        case ALIR_OP_MOD: return "mod";
        case ALIR_OP_FADD: return "fadd";
        case ALIR_OP_FSUB: return "fsub";
        case ALIR_OP_FMUL: return "fmul";
        case ALIR_OP_FDIV: return "fdiv";

        case ALIR_OP_JUMP: return "jump";
        case ALIR_OP_CONDI: return "condition";
        case ALIR_OP_CALL: return "call";
        case ALIR_OP_RET: return "ret";
        case ALIR_OP_PANIC: return "panic";

        case ALIR_OP_CAST: return "cast";
        case ALIR_OP_NOT: return "not";

        case ALIR_OP_LT: return "lt";
        case ALIR_OP_GT: return "gt";
        case ALIR_OP_LTE: return "lte";
        case ALIR_OP_GTE: return "gte";
        case ALIR_OP_EQ: return "eq";
        case ALIR_OP_NEQ: return "neq";

        case ALIR_OP_AND: return "and";
        case ALIR_OP_OR: return "or";
        case ALIR_OP_XOR: return "xor";
        case ALIR_OP_SHL: return "shl";
        case ALIR_OP_SHR: return "shr";
        case ALIR_OP_ROTR: return "rotr";
        case ALIR_OP_ROTL: return "rotl";

        default: return "op";
    }
}
