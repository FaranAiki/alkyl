#include "meta/vm_internal.h"
#include "semantic/semantic.h"
#include <string.h>

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
        
case ALIR_OP_SIZEOF:
case ALIR_OP_ALIGNOF:
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
