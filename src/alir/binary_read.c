#include "alir.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static uint8_t br_u8(FILE *f) { 
    uint8_t v; 
    if (fread(&v, 1, 1, f) != 1) return 0;
    return v; 
}
static uint32_t br_u32(FILE *f) {
    uint32_t result = 0;
    uint32_t shift = 0;
    uint8_t byte;
    while (fread(&byte, 1, 1, f) == 1) {
        result |= ((uint32_t)(byte & 0x7F) << shift);
        if ((byte & 0x80) == 0) break;
        shift += 7;
    }
    return result;
}
static uint64_t br_u64(FILE *f) { 
    uint64_t v; 
    if (fread(&v, 8, 1, f) != 1) return 0;
    return v; 
}

static char* br_str(AlirModule *m, FILE *f) {
    uint32_t len = br_u32(f);
    if (len == 0) return NULL;
    len -= 1;
    char *buf = alir_alloc(m, len + 1);
    if (!buf) return NULL;
    if (len > 0 && fread(buf, 1, len, f) != len) return NULL;
    buf[len] = 0;
    return buf;
}

static VarType br_type(AlirModule *m, FILE *f) {
    VarType t; memset(&t, 0, sizeof(VarType));
    t.base = br_u8(f);
    t.ptr_depth = br_u32(f);
    t.class_name = br_str(m, f);
    t.array_size = br_u32(f);
    t.array_depth = br_u32(f);
    t.fp_param_count = br_u32(f);
    t.is_tainted = br_u8(f);
    t.is_pristine = br_u8(f);
    return t;
}

static AlirValue* br_value(AlirModule *m, FILE *f) {
    uint8_t kind = br_u8(f);
    if (kind == 0xFF) return NULL;
    AlirValue *v = alir_alloc(m, sizeof(AlirValue));
    v->kind = kind;
    v->type = br_type(m, f);
    v->temp_id = br_u32(f);
    
    if (v->kind == ALIR_VAL_INT || v->kind == ALIR_VAL_SINGLE || v->kind == ALIR_VAL_DOUBLE || v->kind == ALIR_VAL_CONST) {
        v->val.int_val = br_u64(f);
    } else if (v->kind == ALIR_VAL_VAR || v->kind == ALIR_VAL_LABEL || v->kind == ALIR_VAL_TYPE || v->kind == ALIR_VAL_GLOBAL) {
        v->val.str_val = br_str(m, f);
    }
    return v;
}

static AlirInst* br_inst(AlirModule *m, FILE *f) {
    AlirInst *i = alir_alloc(m, sizeof(AlirInst));
    i->op = br_u32(f);
    i->dest = br_value(m, f);
    i->op1 = br_value(m, f);
    i->op2 = br_value(m, f);
    i->arg_count = br_u32(f);
    if (i->arg_count > 0) {
        i->args = alir_alloc(m, sizeof(AlirValue*) * i->arg_count);
        for (int j = 0; j < i->arg_count; j++) {
            i->args[j] = br_value(m, f);
        }
    }
    
    uint32_t num_cases = br_u32(f);
    AlirSwitchCase **c_tail = &i->cases;
    for (uint32_t j = 0; j < num_cases; j++) {
        AlirSwitchCase *c = alir_alloc(m, sizeof(AlirSwitchCase));
        c->value = br_u64(f);
        c->label = br_str(m, f);
        *c_tail = c;
        c_tail = &c->next;
    }
    
    i->line = br_u32(f);
    i->col = br_u32(f);
    return i;
}

static AlirBlock* br_block(AlirModule *m, FILE *f) {
    AlirBlock *b = alir_alloc(m, sizeof(AlirBlock));
    b->label = br_str(m, f);
    
    uint32_t inst_c = br_u32(f);
    AlirInst **i_tail = &b->head;
    for (uint32_t j = 0; j < inst_c; j++) {
        AlirInst *i = br_inst(m, f);
        *i_tail = i;
        b->tail = i;
        i_tail = &i->next;
    }
    return b;
}

static AlirFunction* br_func(AlirModule *m, FILE *f) {
    AlirFunction *fn = alir_alloc(m, sizeof(AlirFunction));
    fn->name = br_str(m, f);
    fn->ret_type = br_type(m, f);
    fn->is_flux = br_u8(f);
    fn->is_varargs = br_u8(f);
    fn->cconv = br_str(m, f);
    
    fn->param_count = br_u32(f);
    AlirParam **p_tail = &fn->params;
    for (int j = 0; j < fn->param_count; j++) {
        AlirParam *p = alir_alloc(m, sizeof(AlirParam));
        p->name = br_str(m, f);
        p->type = br_type(m, f);
        *p_tail = p;
        p_tail = &p->next;
    }
    
    uint32_t block_c = br_u32(f);
    AlirBlock **b_tail = &fn->blocks;
    for (uint32_t j = 0; j < block_c; j++) {
        AlirBlock *b = br_block(m, f);
        *b_tail = b;
        b_tail = &b->next;
    }
    fn->block_count = block_c;
    return fn;
}

static AlirStruct* br_struct(AlirModule *m, FILE *f) {
    AlirStruct *st = alir_alloc(m, sizeof(AlirStruct));
    st->name = br_str(m, f);
    st->field_count = br_u32(f);
    
    uint32_t fc = br_u32(f);
    AlirField **fd_tail = &st->fields;
    for (uint32_t j = 0; j < fc; j++) {
        AlirField *fd = alir_alloc(m, sizeof(AlirField));
        fd->name = br_str(m, f);
        fd->type = br_type(m, f);
        fd->index = br_u32(f);
        *fd_tail = fd;
        fd_tail = &fd->next;
    }
    return st;
}

static AlirGlobal* br_global(AlirModule *m, FILE *f) {
    AlirGlobal *g = alir_alloc(m, sizeof(AlirGlobal));
    g->name = br_str(m, f);
    g->type = br_type(m, f);
    g->string_content = br_str(m, f);
    return g;
}

static const uint8_t MAGIC[9] = {0x2f, 0x58, 0xb0, 0x4f, 0x2e, 0xc2, 0xa8, 0xee, 0x24};
static const uint8_t VERSION = 1;

AlirModule* alir_read_binary(CompilerContext *ctx, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    uint8_t magic[9];
    if (fread(magic, 1, 9, f) != 9 || memcmp(magic, MAGIC, 9) != 0) {
        fclose(f);
        return NULL;
    }
    
    uint8_t version;
    if (fread(&version, 1, 1, f) != 1 || version != VERSION) {
        fclose(f);
        return NULL;
    }
    
    AlirModule *mod = alir_create_module(ctx, ""); // name will be overwritten
    mod->name = br_str(mod, f);
    mod->src = NULL;
    mod->filename = br_str(mod, f);
    
    uint32_t g_c = br_u32(f);
    AlirGlobal **g_tail = &mod->globals;
    for (uint32_t i = 0; i < g_c; i++) {
        AlirGlobal *g = br_global(mod, f);
        *g_tail = g;
        g_tail = &g->next;
    }
    
    uint32_t s_c = br_u32(f);
    AlirStruct **s_tail = &mod->structs;
    for (uint32_t i = 0; i < s_c; i++) {
        AlirStruct *st = br_struct(mod, f);
        *s_tail = st;
        s_tail = &st->next;
    }
    
    uint32_t f_c = br_u32(f);
    AlirFunction **f_tail = &mod->functions;
    for (uint32_t i = 0; i < f_c; i++) {
        AlirFunction *fn = br_func(mod, f);
        *f_tail = fn;
        f_tail = &fn->next;
    }
    
    fclose(f);
    return mod;
}
