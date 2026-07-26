#include "meta/vm_internal.h"

void vm_eval_flow(VMContext *ctx, AlirInst *inst) {
    switch(inst->op) {
case ALIR_OP_JUMP: {
                    if (inst->op1 && inst->op1->kind == ALIR_VAL_LABEL) {
                        (*ctx->next_block) = find_block(ctx->func, inst->op1->val.str_val);
                    }
                    break;
                }
                    break;
case ALIR_OP_SWITCH: {
                    long long cond = 0;
                    if (inst->op1->kind == ALIR_VAL_TEMP) cond = ctx->registers[inst->op1->temp_id].as.int_val;
                    else if (inst->op1->kind == ALIR_VAL_CONST) cond = inst->op1->val.long_long_val;
                    
                    AlirSwitchCase *c = inst->cases;
                    int matched = 0;
                    while (c) {
                        if (c->value == cond) {
                            (*ctx->next_block) = find_block(ctx->func, c->label);
                            matched = 1;
                            break;
                        }
                        c = c->next;
                    }
                    if (!matched && inst->op2 && inst->op2->kind == ALIR_VAL_LABEL) {
                        (*ctx->next_block) = find_block(ctx->func, inst->op2->val.str_val);
                    }
                    break;
                }
                    break;
case ALIR_OP_CONDI: {
                    long long cond = 0;
                    if (inst->op1->kind == ALIR_VAL_TEMP) cond = ctx->registers[inst->op1->temp_id].as.int_val;
                    else if (inst->op1->kind == ALIR_VAL_CONST) cond = inst->op1->val.long_long_val;
                    
                    if (cond) {
                        if (inst->op2 && inst->op2->kind == ALIR_VAL_LABEL) 
                            (*ctx->next_block) = find_block(ctx->func, inst->op2->val.str_val);
                    } else {
                        if (inst->arg_count > 0 && inst->args[0]->kind == ALIR_VAL_LABEL)
                            (*ctx->next_block) = find_block(ctx->func, inst->args[0]->val.str_val);
                    }
                    break;
                }
                    break;
case ALIR_OP_RET: {
                    if (inst->op1) {
                        if (inst->op1->kind == ALIR_VAL_TEMP) { 
                            if (inst->op1->type.base == TYPE_SINGLE) {
                                float f = (float)ctx->registers[inst->op1->temp_id].as.single_val;
                                memcpy(&(*ctx->ret_val), &f, sizeof(float));
                            } else if (inst->op1->type.base == TYPE_DOUBLE) {
                                double d = ctx->registers[inst->op1->temp_id].as.single_val;
                                memcpy(&(*ctx->ret_val), &d, sizeof(double));
                            } else {
                                (*ctx->ret_val) = ctx->registers[inst->op1->temp_id].as.int_val; 
                            }
                            ctx->should_return = 1; return;
                        }
                        else if (inst->op1->kind == ALIR_VAL_CONST) { 
                            if (inst->op1->type.base == TYPE_SINGLE) {
                                float f = inst->op1->val.single_val;
                                memcpy(&(*ctx->ret_val), &f, sizeof(float));
                            } else if (inst->op1->type.base == TYPE_DOUBLE) {
                                double d = inst->op1->val.double_val;
                                memcpy(&(*ctx->ret_val), &d, sizeof(double));
                            } else {
                                (*ctx->ret_val) = inst->op1->val.long_long_val;
                            }
                            ctx->should_return = 1; return;
                        }
                        else if (inst->op1->kind == ALIR_VAL_GLOBAL) {
                            if (ctx->vm) {
                                VMGlobal *vg = ctx->vm->globals;
                                while(vg) {
                                    if (strcmp(vg->name, inst->op1->val.str_val) == 0) {
                                        (*ctx->ret_val) = (long long)(intptr_t)vg->ptr_val;
                                        ctx->should_return = 1; return;
                                    }
                                    vg = vg->next;
                                }
                            }
                            if (ctx->module) {
                                AlirGlobal *g = ctx->module->globals;
                                while(g) {
                                    if (strcmp(g->name, inst->op1->val.str_val) == 0 && g->string_content) {
                                        (*ctx->ret_val) = (intptr_t)g->string_content;
                                        ctx->should_return = 1; return;
                                    }
                                    g = g->next;
                                }
                            }
                        }
                    }
                    (*ctx->ret_val) = 0; ctx->should_return = 1; return;
                }
                    break;
        
case ALIR_OP_YIELD:
    // Yield not fully implemented in MetaVM, treat as a break/return for now
    break;

default: break;
    }
}
