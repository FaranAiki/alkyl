#include "meta/vm_internal.h"
#include "semantic/semantic.h"
#include <string.h>
#include "common/diagnostic.h"

void vm_eval_math(VMContext *ctx, AlirInst *inst) {
    long long v1 = 0, v2 = 0;
    switch(inst->op) {
        case ALIR_OP_ADD:
        case ALIR_OP_SUB:
        case ALIR_OP_MUL:
        case ALIR_OP_DIV:
        case ALIR_OP_MOD:
        case ALIR_OP_AND:
        case ALIR_OP_OR:
        case ALIR_OP_XOR:
        case ALIR_OP_SHL:
        case ALIR_OP_SHR:
        case ALIR_OP_ROTL:
        case ALIR_OP_ROTR:
        case ALIR_OP_LT:
        case ALIR_OP_GT:
        case ALIR_OP_LTE:
        case ALIR_OP_GTE:
        case ALIR_OP_EQ:
        case ALIR_OP_NOT:
        case ALIR_OP_NEQ: {
            if (inst->dest && inst->op1 && inst->op2) {
                
                if (inst->op1->kind == ALIR_VAL_CONST) {
                    if (inst->op1->type.base == TYPE_SINGLE) { v1 = inst->op1->val.single_val; }
                    else if (inst->op1->type.base == TYPE_DOUBLE) { v1 = inst->op1->val.double_val; }
                    else { v1 = inst->op1->val.long_long_val; }
                } else if (inst->op1->kind == ALIR_VAL_TEMP) {
                    if (inst->op1->type.base == TYPE_SINGLE) { v1 = ctx->registers[inst->op1->temp_id].as.single_val; }
                    else { v1 = ctx->registers[inst->op1->temp_id].as.int_val; }
                } else if (inst->op1->kind == ALIR_VAL_VAR) {
                    v1 = meta_vm_resolve_var(inst->op1, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                } else {
                    if (inst->op1->type.base == TYPE_SINGLE || inst->op1->type.base == TYPE_DOUBLE) {
                        v1 = inst->op1->val.single_val;
                    } else {
                        v1 = inst->op1->val.long_long_val;
                    }
                }
                
                if (inst->op2->kind == ALIR_VAL_CONST) {
                    if (inst->op2->type.base == TYPE_SINGLE) { v2 = inst->op2->val.single_val; }
                    else if (inst->op2->type.base == TYPE_DOUBLE) { v2 = inst->op2->val.double_val; }
                    else { v2 = inst->op2->val.long_long_val; }
                } else if (inst->op2->kind == ALIR_VAL_TEMP) {
                    if (inst->op2->type.base == TYPE_SINGLE) { v2 = ctx->registers[inst->op2->temp_id].as.single_val; }
                    else { v2 = ctx->registers[inst->op2->temp_id].as.int_val; }
                } else if (inst->op2->kind == ALIR_VAL_VAR) {
                    v2 = meta_vm_resolve_var(inst->op2, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                } else {
                    if (inst->op2->type.base == TYPE_SINGLE || inst->op2->type.base == TYPE_DOUBLE) {
                        v2 = inst->op2->val.single_val;
                    } else {
                        v2 = inst->op2->val.long_long_val;
                    }
                }
                
                long long res = 0;
                if (inst->op == ALIR_OP_ADD) res = v1 + v2;
                else if (inst->op == ALIR_OP_NOT) res = ~v1;
                else if (inst->op == ALIR_OP_SUB) res = v1 - v2;
                else if (inst->op == ALIR_OP_MUL) res = v1 * v2;
                else if (inst->op == ALIR_OP_DIV) {
                    if (v2 != 0) res = v1 / v2;
                    else {
                        if (ctx->sem_ctx) {
                            ASTNode fake_node = {0};
                            fake_node.line = inst->line;
                            fake_node.col = inst->col;
                            sem_error(ctx->sem_ctx, &fake_node, "Division by zero in MetaVM");
                        } else {
                            fprintf(stderr, "Division by zero\n");
                        }
                        ctx->vm->status = -1;
                        *ctx->ret_val = ctx->vm->status;
                        ctx->should_return = 1;
                        return;
                    }
                }
                else if (inst->op == ALIR_OP_MOD) {
                    if (v2 != 0) res = v1 % v2;
                    else {
                        if (ctx->sem_ctx) {
                            ASTNode fake_node = {0};
                            fake_node.line = inst->line;
                            fake_node.col = inst->col;
                            sem_error(ctx->sem_ctx, &fake_node, "Modulo by zero in MetaVM");
                        } else {
                            fprintf(stderr, "Modulo by zero\n");
                        }
                        ctx->vm->status = -1;
                        *ctx->ret_val = ctx->vm->status;
                        ctx->should_return = 1;
                        return;
                    }
                }
                else if (inst->op == ALIR_OP_AND) res = v1 & v2;
                else if (inst->op == ALIR_OP_OR) res = v1 | v2;
                else if (inst->op == ALIR_OP_XOR) res = v1 ^ v2;
                else if (inst->op == ALIR_OP_SHL) res = v1 << v2;
                else if (inst->op == ALIR_OP_SHR) res = v1 >> v2;
                else if (inst->op == ALIR_OP_ROTL) {
                    int bw = (inst->op1->type.base == TYPE_INT || inst->op1->type.base == TYPE_AUTO) ? 32 : 64;
                    if (bw == 32) {
                        unsigned int u1 = (unsigned int)v1;
                        unsigned int u2 = (unsigned int)v2 % 32;
                        res = (u1 << u2) | (u1 >> (32 - u2));
                    } else {
                        unsigned long long u1 = (unsigned long long)v1;
                        unsigned long long u2 = (unsigned long long)v2 % 64;
                        res = (u1 << u2) | (u1 >> (64 - u2));
                    }
                }
                else if (inst->op == ALIR_OP_ROTR) {
                    int bw = (inst->op1->type.base == TYPE_INT || inst->op1->type.base == TYPE_AUTO) ? 32 : 64;
                    if (bw == 32) {
                        unsigned int u1 = (unsigned int)v1;
                        unsigned int u2 = (unsigned int)v2 % 32;
                        res = (u1 >> u2) | (u1 << (32 - u2));
                    } else {
                        unsigned long long u1 = (unsigned long long)v1;
                        unsigned long long u2 = (unsigned long long)v2 % 64;
                        res = (u1 >> u2) | (u1 << (64 - u2));
                    }
                }
                else if (inst->op == ALIR_OP_LT) res = v1 < v2;
                else if (inst->op == ALIR_OP_GT) res = v1 > v2;
                else if (inst->op == ALIR_OP_LTE) res = v1 <= v2;
                else if (inst->op == ALIR_OP_GTE) res = v1 >= v2;
                else if (inst->op == ALIR_OP_EQ) res = v1 == v2;
                else if (inst->op == ALIR_OP_NEQ) res = v1 != v2;
                
                if (inst->dest->type.base == TYPE_SINGLE || inst->dest->type.base == TYPE_DOUBLE) {
                    ctx->registers[inst->dest->temp_id].as.single_val = res;
                } else {
                    ctx->registers[inst->dest->temp_id].as.int_val = res;
                }
            }
            break;
        }
        case ALIR_OP_FADD:
        case ALIR_OP_FSUB:
        case ALIR_OP_FMUL:
        case ALIR_OP_FDIV: {
            if (inst->dest && inst->op1 && inst->op2) {
                double v1 = 0, v2 = 0;
                
                if (inst->op1->kind == ALIR_VAL_CONST) {
                    if (inst->op1->type.base == TYPE_SINGLE) { v1 = inst->op1->val.single_val; }
                    else { v1 = inst->op1->val.double_val; }
                } else if (inst->op1->kind == ALIR_VAL_TEMP) {
                    if (inst->op1->type.base == TYPE_SINGLE) { v1 = ctx->registers[inst->op1->temp_id].as.single_val; }
                    else { v1 = ctx->registers[inst->op1->temp_id].as.single_val; }
                }
                
                if (inst->op2->kind == ALIR_VAL_CONST) {
                    if (inst->op2->type.base == TYPE_SINGLE) { v2 = inst->op2->val.single_val; }
                    else { v2 = inst->op2->val.double_val; }
                } else if (inst->op2->kind == ALIR_VAL_TEMP) {
                    if (inst->op2->type.base == TYPE_SINGLE) { v2 = ctx->registers[inst->op2->temp_id].as.single_val; }
                    else { v2 = ctx->registers[inst->op2->temp_id].as.single_val; }
                }
                
                double res = 0;
                if (inst->op == ALIR_OP_FADD) res = v1 + v2;
                else if (inst->op == ALIR_OP_FSUB) res = v1 - v2;
                else if (inst->op == ALIR_OP_FMUL) res = v1 * v2;
                else if (inst->op == ALIR_OP_FDIV) res = v1 / v2;
                
                if (inst->dest->type.base == TYPE_SINGLE) {
                    ctx->registers[inst->dest->temp_id].as.single_val = res;
                } else {
                    ctx->registers[inst->dest->temp_id].as.single_val = res;
                }
            }
            break;
        }
        default: break;
    }
}
