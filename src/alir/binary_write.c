#include "alir.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static void bw_u8(FILE *f, uint8_t v) { fwrite(&v, 1, 1, f); }
static void bw_u32(FILE *f, uint32_t v) {
    do {
        uint8_t byte = v & 0x7F;
        v >>= 7;
        if (v != 0) byte |= 0x80;
        fwrite(&byte, 1, 1, f);
    } while (v != 0);
}
static void bw_u64(FILE *f, uint64_t v) { fwrite(&v, 8, 1, f); }

static void bw_str(FILE *f, const char *s) {
    if (!s) { bw_u32(f, 0); return; }
    uint32_t len = strlen(s);
    bw_u32(f, len + 1);
    if (len > 0) fwrite(s, 1, len, f);
}

static void bw_type(FILE *f, VarType t) {
    bw_u8(f, t.base);
    bw_u32(f, t.ptr_depth);
    bw_str(f, t.class_name);
    bw_u32(f, t.array_size);
    bw_u32(f, t.array_depth);
    bw_u32(f, t.fp_param_count);
    bw_u8(f, t.is_tainted);
    bw_u8(f, t.is_pristine);
    // Note: ignoring fp_ret_type and fp_param_types for now to avoid complexity unless fully needed
    // The spec usually lowers function types into pointers
}

static void bw_value(FILE *f, AlirValue *v) {
    if (!v) { bw_u8(f, 0xFF); return; }
    bw_u8(f, v->kind);
    bw_type(f, v->type);
    bw_u32(f, v->temp_id);
    // Value union
    if (v->kind == ALIR_VAL_INT || v->kind == ALIR_VAL_FLOAT || v->kind == ALIR_VAL_CONST) {
        bw_u64(f, v->val.int_val); // union overlays float, we just copy 8 bytes
    } else if (v->kind == ALIR_VAL_VAR || v->kind == ALIR_VAL_LABEL || v->kind == ALIR_VAL_TYPE || v->kind == ALIR_VAL_GLOBAL) {
        bw_str(f, v->val.str_val);
    }
}

static void bw_inst(FILE *f, AlirInst *i) {
    bw_u32(f, i->op);
    bw_value(f, i->dest);
    bw_value(f, i->op1);
    bw_value(f, i->op2);
    bw_u32(f, i->arg_count);
    for (int j = 0; j < i->arg_count; j++) {
        bw_value(f, i->args[j]);
    }
    
    // cases
    uint32_t num_cases = 0;
    AlirSwitchCase *c = i->cases;
    while(c) { num_cases++; c = c->next; }
    bw_u32(f, num_cases);
    c = i->cases;
    while(c) {
        bw_u64(f, c->value);
        bw_str(f, c->label);
        c = c->next;
    }
    
    bw_u32(f, i->line);
    bw_u32(f, i->col);
}

static void bw_block(FILE *f, AlirBlock *b) {
    if (!b) return;
    bw_str(f, b->label);
    
    uint32_t inst_c = 0;
    AlirInst *i = b->head;
    while(i) { inst_c++; i = i->next; }
    bw_u32(f, inst_c);
    
    i = b->head;
    while(i) { bw_inst(f, i); i = i->next; }
}

static void bw_func(FILE *f, AlirFunction *fn) {
    bw_str(f, fn->name);
    bw_type(f, fn->ret_type);
    bw_u8(f, fn->is_flux);
    bw_u8(f, fn->is_varargs);
    bw_str(f, fn->cconv);
    
    bw_u32(f, fn->param_count);
    AlirParam *p = fn->params;
    while(p) {
        bw_str(f, p->name);
        bw_type(f, p->type);
        p = p->next;
    }
    
    uint32_t block_c = 0;
    AlirBlock *b = fn->blocks;
    while(b) { block_c++; b = b->next; }
    bw_u32(f, block_c);
    
    b = fn->blocks;
    while(b) { bw_block(f, b); b = b->next; }
}

static void bw_struct(FILE *f, AlirStruct *st) {
    bw_str(f, st->name);
    bw_u32(f, st->field_count);
    AlirField *fd = st->fields;
    uint32_t fc = 0;
    while(fd) { fc++; fd = fd->next; }
    bw_u32(f, fc);
    fd = st->fields;
    while(fd) {
        bw_str(f, fd->name);
        bw_type(f, fd->type);
        bw_u32(f, fd->index);
        fd = fd->next;
    }
}

static void bw_global(FILE *f, AlirGlobal *g) {
    bw_str(f, g->name);
    bw_type(f, g->type);
    bw_str(f, g->string_content);
}

static const uint8_t MAGIC[9] = {0x2f, 0x58, 0xb0, 0x4f, 0x2e, 0xc2, 0xa8, 0xee, 0x24};
static const uint8_t VERSION = 1;

int alir_write_binary(AlirModule *mod, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) return 1;

    fwrite(MAGIC, 1, 9, f);
    fwrite(&VERSION, 1, 1, f);
    
    bw_str(f, mod->name);
    bw_str(f, mod->filename);
    
    uint32_t g_c = 0; AlirGlobal *g = mod->globals; while(g) { g_c++; g = g->next; } bw_u32(f, g_c);
    g = mod->globals; while(g) { bw_global(f, g); g = g->next; }
    
    uint32_t s_c = 0; AlirStruct *st = mod->structs; while(st) { s_c++; st = st->next; } bw_u32(f, s_c);
    st = mod->structs; while(st) { bw_struct(f, st); st = st->next; }
    
    uint32_t f_c = 0; AlirFunction *fn = mod->functions; while(fn) { f_c++; fn = fn->next; } bw_u32(f, f_c);
    fn = mod->functions; while(fn) { bw_func(f, fn); fn = fn->next; }
    
    fclose(f);
    return 0;
}
