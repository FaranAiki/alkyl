#include "alir.h"
#include <stdlib.h>
#include <string.h>

AlirModule* alir_create_module(const char *name) {
    AlirModule *m = calloc(1, sizeof(AlirModule));
    m->name = strdup(name);
    return m;
}

AlirFunction* alir_add_function(AlirModule *mod, const char *name, VarType ret, int is_flux) {
    AlirFunction *f = calloc(1, sizeof(AlirFunction));
    f->name = strdup(name);
    f->ret_type = ret;
    f->is_flux = is_flux;
    
    if (!mod->functions) {
        mod->functions = f;
    } else {
        AlirFunction *curr = mod->functions;
        while(curr->next) curr = curr->next;
        curr->next = f;
    }
    return f;
}

void alir_func_add_param(AlirFunction *func, const char *name, VarType type) {
    AlirParam *p = calloc(1, sizeof(AlirParam));
    p->name = strdup(name ? name : "");
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

// Add string to global pool
AlirValue* alir_module_add_string_literal(AlirModule *mod, const char *content, int id_hint) {
    char label[64];
    sprintf(label, "str.%d", id_hint);
    
    AlirGlobal *g = calloc(1, sizeof(AlirGlobal));
    g->name = strdup(label);
    g->string_content = strdup(content);
    g->type = (VarType){TYPE_STRING, 0, NULL, 0, 0};
    
    g->next = mod->globals;
    mod->globals = g;
    
    return alir_val_global(label, g->type);
}

AlirBlock* alir_add_block(AlirFunction *func, const char *label_hint) {
    AlirBlock *b = calloc(1, sizeof(AlirBlock));
    int global_id = 0;
    b->id = global_id++;
    
    if (label_hint) b->label = strdup(label_hint);
    else {
        char buf[32];
        sprintf(buf, "L%d", b->id);
        b->label = strdup(buf);
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

void alir_register_struct(AlirModule *mod, const char *name, AlirField *fields) {
    AlirStruct *st = calloc(1, sizeof(AlirStruct));
    st->name = strdup(name);
    st->fields = fields;
    
    AlirField *f = fields;
    while(f) {
        st->field_count++;
        f = f->next;
    }
    
    st->next = mod->structs;
    mod->structs = st;
}

AlirStruct* alir_find_struct(AlirModule *mod, const char *name) {
    AlirStruct *curr = mod->structs;
    while(curr) {
        if (strcmp(curr->name, name) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

int alir_get_field_index(AlirModule *mod, const char *struct_name, const char *field_name) {
    AlirStruct *st = alir_find_struct(mod, struct_name);
    if (!st) return -1;
    
    AlirField *f = st->fields;
    while(f) {
        if (strcmp(f->name, field_name) == 0) return f->index;
        f = f->next;
    }
    return -1;
}

const char* alir_op_str(AlirOpcode op) {
    switch(op) {
        case ALIR_OP_ALLOCA: return "alloca";
        case ALIR_OP_STORE: return "store";
        case ALIR_OP_LOAD: return "load";
        case ALIR_OP_GET_PTR: return "getptr";
        case ALIR_OP_BITCAST: return "bitcast";
        
        case ALIR_OP_ALLOC_HEAP: return "halloc";
        case ALIR_OP_SIZEOF: return "sizeof";
        case ALIR_OP_FREE: return "free";
        
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
        case ALIR_OP_CONDI: return "condi";
        case ALIR_OP_SWITCH: return "switch";
        case ALIR_OP_CALL: return "call";
        case ALIR_OP_RET: return "ret";
        
        // Added flux/iterator support strings
        case ALIR_OP_YIELD: return "yield";
        case ALIR_OP_ITER_INIT: return "iter_init";
        case ALIR_OP_ITER_VALID: return "iter_valid";
        case ALIR_OP_ITER_NEXT: return "iter_next";
        case ALIR_OP_ITER_GET: return "iter_get";
        
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
        
        default: return "op";
    }
}

