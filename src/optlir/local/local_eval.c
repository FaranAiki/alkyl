#include "optlir.h"
#include "optlir/local.h"
#include "optlir/local_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

ConstVal get_const_for_value(AlirValue *val) {
    ConstVal res = {0};
    if (!val) return res;
    
    if (val->kind == ALIR_VAL_CONST) {
        res.is_const = 1;
        res.is_float = (val->type.base == TYPE_SINGLE || val->type.base == TYPE_DOUBLE);
        if (res.is_float) {
            if (val->type.base == TYPE_SINGLE) res.double_val = val->val.single_val;
            else res.double_val = val->val.double_val;
        } else {
            res.int_val = val->val.long_long_val;
        }
        return res;
    }
    
    return res;
}

ConstVal eval_const_binary(int op, ConstVal l, ConstVal r, VarType type) {
    ConstVal res = {0};
    res.is_float = (type.base == TYPE_SINGLE || type.base == TYPE_DOUBLE);
    
    if (l.is_float || r.is_float) {
        double d1 = l.is_float ? l.double_val : (double)l.int_val;
        double d2 = r.is_float ? r.double_val : (double)r.int_val;
        switch (op) {
            case ALIR_OP_ADD: res.double_val = d1 + d2; break;
            case ALIR_OP_SUB: res.double_val = d1 - d2; break;
            case ALIR_OP_MUL: res.double_val = d1 * d2; break;
            case ALIR_OP_DIV: res.double_val = d2 != 0 ? d1 / d2 : 0; break;
            case ALIR_OP_FADD: res.double_val = d1 + d2; break;
            case ALIR_OP_FSUB: res.double_val = d1 - d2; break;
            case ALIR_OP_FMUL: res.double_val = d1 * d2; break;
            case ALIR_OP_FDIV: res.double_val = d2 != 0 ? d1 / d2 : 0; break;
            default: res.is_const = 0; return res;
        }
        res.is_const = 1;
        return res;
    }
    
    long long v1 = l.int_val, v2 = r.int_val;
    switch (op) {
        case ALIR_OP_ADD: res.int_val = v1 + v2; break;
        case ALIR_OP_SUB: res.int_val = v1 - v2; break;
        case ALIR_OP_MUL: res.int_val = v1 * v2; break;
        case ALIR_OP_DIV: res.int_val = v2 != 0 ? v1 / v2 : 0; break;
        case ALIR_OP_MOD: res.int_val = v2 != 0 ? v1 % v2 : 0; break;
        case ALIR_OP_AND: res.int_val = v1 & v2; break;
        case ALIR_OP_OR:  res.int_val = v1 | v2; break;
        case ALIR_OP_XOR: res.int_val = v1 ^ v2; break;
        case ALIR_OP_SHL: res.int_val = v1 << v2; break;
        case ALIR_OP_SHR: res.int_val = v1 >> v2; break;
        case ALIR_OP_LT:  res.int_val = v1 < v2; break;
        case ALIR_OP_GT:  res.int_val = v1 > v2; break;
        case ALIR_OP_LTE: res.int_val = v1 <= v2; break;
        case ALIR_OP_GTE: res.int_val = v1 >= v2; break;
        case ALIR_OP_EQ:  res.int_val = v1 == v2; break;
        case ALIR_OP_NEQ: res.int_val = v1 != v2; break;
        default: res.is_const = 0; return res;
    }
    res.is_const = 1;
    return res;
}

ConstVal eval_const_unary(int op, ConstVal v, VarType type) {
    ConstVal res = {0};
    res.is_float = (type.base == TYPE_SINGLE || type.base == TYPE_DOUBLE);
    
    if (res.is_float) {
        double d = v.is_float ? v.double_val : (double)v.int_val;
        switch (op) {
            case ALIR_OP_NOT: res.double_val = ~(long long)d; break;
            default: res.is_const = 0; return res;
        }
        res.is_const = 1;
        return res;
    }
    
    switch (op) {
        case ALIR_OP_NOT: res.int_val = ~v.int_val; break;
        default: res.is_const = 0; return res;
    }
    res.is_const = 1;
    return res;
}

int is_identity_op(int op, ConstVal c) {
    if (!c.is_const) return 0;
    if (c.int_val == 0 && (op == ALIR_OP_ADD || op == ALIR_OP_SUB || op == ALIR_OP_OR || op == ALIR_OP_XOR)) return 1;
    if (c.int_val == 1 && (op == ALIR_OP_MUL || op == ALIR_OP_DIV || op == ALIR_OP_MOD)) return 1;
    if (c.int_val == -1 && (op == ALIR_OP_AND)) return 1;
    if (c.int_val == 0 && (op == ALIR_OP_MUL)) return 1;
    return 0;
}

int is_self_cancel_op(int op) {
    return (op == ALIR_OP_SUB || op == ALIR_OP_XOR);
}

void constant_propagate_function(AlirModule *module, AlirFunction *func) {
    (void)module;
    if (!func || !func->blocks) return;
    
    AlirBlock *b = func->blocks;
    while (b) {
        AlirInst *prev = NULL;
        AlirInst *i = b->head;
        while (i) {
            AlirInst *next = i->next;
            int removed = 0;
            
            if (i->dest && i->op >= ALIR_OP_ADD && i->op <= ALIR_OP_NEQ) {
                ConstVal l = get_const_for_value(i->op1);
                ConstVal r = get_const_for_value(i->op2);
                
                if (l.is_const && r.is_const) {
                    ConstVal res = eval_const_binary(i->op, l, r, i->dest->type);
                    if (res.is_const) {
                        i->dest->kind = ALIR_VAL_CONST;
                        if (res.is_float) {
                            if (i->dest->type.base == TYPE_SINGLE) i->dest->val.single_val = (float)res.double_val;
                            else i->dest->val.double_val = res.double_val;
                        } else {
                            i->dest->val.long_long_val = res.int_val;
                        }
                        remove_instruction(b, prev, i);
                        removed = 1;
                    }
                } else if (l.is_const && is_identity_op(i->op, l)) {
                    i->dest->kind = ALIR_VAL_TEMP;
                    i->dest->temp_id = i->op2->temp_id;
                    remove_instruction(b, prev, i);
                    removed = 1;
                } else if (r.is_const && is_identity_op(i->op, r)) {
                    i->dest->kind = ALIR_VAL_TEMP;
                    i->dest->temp_id = i->op1->temp_id;
                    remove_instruction(b, prev, i);
                    removed = 1;
                } else if (l.is_const && r.is_const && is_self_cancel_op(i->op)) {
                    if (i->dest->type.base == TYPE_SINGLE) {
                        i->dest->kind = ALIR_VAL_CONST;
                        i->dest->val.single_val = 0.0f;
                    } else if (i->dest->type.base == TYPE_DOUBLE) {
                        i->dest->kind = ALIR_VAL_CONST;
                        i->dest->val.double_val = 0.0;
                    } else {
                        i->dest->kind = ALIR_VAL_CONST;
                        i->dest->val.long_long_val = 0;
                    }
                    remove_instruction(b, prev, i);
                    removed = 1;
                }
            }
            
            if (!removed && i->op == ALIR_OP_NOT && i->op1) {
                ConstVal v = get_const_for_value(i->op1);
                if (v.is_const) {
                    ConstVal res = eval_const_unary(ALIR_OP_NOT, v, i->dest->type);
                    if (res.is_const) {
                        i->dest->kind = ALIR_VAL_CONST;
                        if (res.is_float) {
                            if (i->dest->type.base == TYPE_SINGLE) i->dest->val.single_val = (float)res.double_val;
                            else i->dest->val.double_val = res.double_val;
                        } else {
                            i->dest->val.long_long_val = res.int_val;
                        }
                        remove_instruction(b, prev, i);
                        removed = 1;
                    }
                }
            }

            if (!removed && i->op == ALIR_OP_CAST && i->op1) {
                ConstVal v = get_const_for_value(i->op1);
                if (v.is_const) {
                    ConstVal res = v;
                    if (v.is_float) {
                        if (i->dest->type.base == TYPE_SINGLE) res.double_val = (float)v.double_val;
                        else if (i->dest->type.base == TYPE_DOUBLE) res.double_val = v.double_val;
                        else res.int_val = (long long)v.double_val;
                    } else {
                        if (i->dest->type.base == TYPE_SINGLE) res.double_val = (float)v.int_val;
                        else if (i->dest->type.base == TYPE_DOUBLE) res.double_val = (double)v.int_val;
                        else res.int_val = v.int_val;
                    }
                    res.is_const = 1;
                    res.is_float = (i->dest->type.base == TYPE_SINGLE || i->dest->type.base == TYPE_DOUBLE);
                    i->dest->kind = ALIR_VAL_CONST;
                    if (res.is_float) {
                        if (i->dest->type.base == TYPE_SINGLE) i->dest->val.single_val = (float)res.double_val;
                        else i->dest->val.double_val = res.double_val;
                    } else {
                        i->dest->val.long_long_val = res.int_val;
                    }
                    remove_instruction(b, prev, i);
                    removed = 1;
                }
            }
            
            if (!removed) {
                prev = i;
            }
            i = next;
        }
        b = b->next;
    }
}

int is_temp_used_except_in_load(AlirFunction *func, AlirValue *temp, AlirInst *exclude_load) {
    if (!func || !temp || temp->kind != ALIR_VAL_TEMP) return 1;
    
    AlirBlock *b = func->blocks;
    while (b) {
        AlirInst *i = b->head;
        while (i) {
            if (i == exclude_load) {
                i = i->next;
                continue;
            }
            if (i->op1 == temp) return 1;
            if (i->op2 == temp) return 1;
            if (i->dest == temp) return 1;
            for (int j = 0; j < i->arg_count; j++) {
                if (i->args[j] == temp) return 1;
            }
            i = i->next;
        }
        b = b->next;
    }
    return 0;
}

int all_args_const(AlirInst *inst) {
    if (!inst || inst->arg_count <= 0) return 1;
    for (int i = 0; i < inst->arg_count; i++) {
        if (!inst->args[i]) return 0;
        if (inst->args[i]->kind != ALIR_VAL_CONST) return 0;
    }
    return 1;
}

void eval_pure_call_function(AlirModule *module, AlirFunction *func) {
    if (!func || !func->blocks || func->is_extern || !func->is_pure) return;
    
    AlirBlock *b = func->blocks;
    while (b) {
        AlirInst *prev = NULL;
        AlirInst *i = b->head;
        while (i) {
            AlirInst *next = i->next;
            int removed = 0;
            
            if (i->op == ALIR_OP_CALL && i->op1 && i->op1->kind == ALIR_VAL_VAR && i->dest && all_args_const(i)) {
                AlirFunction *callee = module->functions;
                while (callee) {
                    if (strcmp(callee->name, i->op1->val.str_val) == 0 && callee->is_pure && !callee->is_extern && callee->block_count > 0) {
                        ConstVal res = eval_pure_function(module, callee, i->args, i->arg_count, i->dest->type);
                        if (res.is_const) {
                            i->dest->kind = ALIR_VAL_CONST;
                            if (res.is_float) {
                                if (i->dest->type.base == TYPE_SINGLE) i->dest->val.single_val = (float)res.double_val;
                                else i->dest->val.double_val = res.double_val;
                            } else {
                                i->dest->val.long_long_val = res.int_val;
                            }
                            remove_instruction(b, prev, i);
                            removed = 1;
                        }
                        break;
                    }
                    callee = callee->next;
                }
            }
            
            if (!removed) {
                prev = i;
            }
            i = next;
        }
        b = b->next;
    }
}