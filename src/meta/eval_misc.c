#include "meta/vm_internal.h"
#include "semantic/semantic.h"
#include <string.h>

static int get_type_size(VarType t) {
    fprintf(stderr, "DEBUG get_type_size: base=%d ptr_depth=%d array_size=%d\n", t.base, t.ptr_depth, t.array_size);
    int base_size = 8;
    if (t.ptr_depth > 0) {
        base_size = 8;
    } else {
        switch(t.base) {
            case TYPE_BOOL:
            case TYPE_CHAR:
            case TYPE_UNSIGNED_CHAR: base_size = 1; break;
            case TYPE_SHORT: base_size = 2; break;
            case TYPE_INT:
            case TYPE_UNSIGNED_INT:
            case TYPE_SINGLE: base_size = 4; break;
            case TYPE_LONG:
            case TYPE_UNSIGNED_LONG:
            case TYPE_LONG_LONG:
            case TYPE_UNSIGNED_LONG_LONG:
            case TYPE_DOUBLE: base_size = 8; break;
            default: base_size = 8; break;
        }
    }
    
    if (t.array_size > 0) {
        if (t.array_depth > 0) {
            return base_size * t.array_size * t.array_depth;
        }
        return base_size * t.array_size;
    }
    return base_size;
}

void vm_eval_misc(VMContext *ctx, AlirInst *inst) {
    switch(inst->op) {
case ALIR_OP_DEFINED: {
                    int is_defined = 0;
                    if (inst->op1 && inst->op1->kind == ALIR_VAL_VAR) {
                        const char *sym_name = inst->op1->val.str_val;
                        
                        // Check globals
                        if (ctx->module) {
                            AlirGlobal *g = ctx->module->globals;
                            while (g) {
                                if (strcmp(g->name, sym_name) == 0) { is_defined = 1; break; }
                                g = g->next;
                            }
                            if (!is_defined) {
                                AlirFunction *f = ctx->module->functions;
                                while (f) {
                                    if (strcmp(f->name, sym_name) == 0) { is_defined = 1; break; }
                                    f = f->next;
                                }
                            }
                        }
                        // Check semantics and macros
                        if (!is_defined && ctx->sem_ctx) {
                            SemScope *dummy;
                            if (sem_symbol_lookup((SemanticCtx*)ctx->sem_ctx, sym_name, &dummy)) {
                                is_defined = 1;
                            }
                            if (!is_defined) {
                                SemanticCtx *sctx = (SemanticCtx*)ctx->sem_ctx;
                                if (sctx->compiler_ctx && sctx->compiler_ctx->macro_head) {
                                    struct MacroDummy {
                                        char *name;
                                        char **params;
                                        int param_count;
                                        void *body;
                                        int body_len;
                                        struct MacroDummy *next;
                                    };
                                    struct MacroDummy *m = (struct MacroDummy *)sctx->compiler_ctx->macro_head;
                                    while (m) {
                                        if (strcmp(m->name, sym_name) == 0) { is_defined = 1; break; }
                                        m = m->next;
                                    }
                                }
                            }
                        }
                    }
                    if (inst->dest) {
                        ctx->registers[inst->dest->temp_id].as.int_val = is_defined;
                    }
                    break;
                }
                    break;
        
case ALIR_OP_SIZEOF: {
    if (inst->dest && inst->op1) {
        ctx->registers[inst->dest->temp_id].as.int_val = get_type_size(inst->op1->type);
    }
    break;
}
case ALIR_OP_ALIGNOF: {
    if (inst->dest && inst->op1) {
        int sz = get_type_size(inst->op1->type);
        if (inst->op1->type.array_size > 0) sz = get_type_size((VarType){ .base = inst->op1->type.base, .ptr_depth = inst->op1->type.ptr_depth });
        ctx->registers[inst->dest->temp_id].as.int_val = sz;
    }
    break;
}
case ALIR_OP_TYPEOF:
case ALIR_OP_FALLBACK:
case ALIR_OP_ITER_INIT:
case ALIR_OP_ITER_VALID:
case ALIR_OP_ITER_NEXT:
case ALIR_OP_ITER_GET:
    break; // Nothing to do or not fully supported in REPL VM at compile time

case ALIR_OP_MOV:
case ALIR_OP_CAST:
case ALIR_OP_BITCAST: {
    if (inst->dest && inst->op1) {
        if (inst->op1->kind == ALIR_VAL_TEMP) {
            ctx->registers[inst->dest->temp_id] = ctx->registers[inst->op1->temp_id];
        } else if (inst->op1->kind == ALIR_VAL_CONST) {
            if (inst->op1->type.base == TYPE_SINGLE) ctx->registers[inst->dest->temp_id].as.single_val = inst->op1->val.single_val;
            else if (inst->op1->type.base == TYPE_DOUBLE) ctx->registers[inst->dest->temp_id].as.single_val = inst->op1->val.double_val;
            else ctx->registers[inst->dest->temp_id].as.int_val = inst->op1->val.long_long_val;
        }
    }
    break;
}

default: break;
    }
}
