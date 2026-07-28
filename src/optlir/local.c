#include "optlir.h"
#include "optlir/local.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct AlirModule AlirModule;
typedef struct AlirFunction AlirFunction;
typedef struct AlirBlock AlirBlock;
typedef struct AlirInst AlirInst;
typedef struct AlirValue AlirValue;
typedef struct AlirGlobal AlirGlobal;
typedef struct AlirStruct AlirStruct;
typedef struct AlirParam AlirParam;

typedef struct ConstVal {
    long long int_val;
    double double_val;
    int is_const;
    int is_float;
} ConstVal;

static ConstVal get_const_for_value(AlirValue *val) {
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

static ConstVal eval_const_binary(int op, ConstVal l, ConstVal r, VarType type) {
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

static ConstVal eval_const_unary(int op, ConstVal v, VarType type) {
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

static int is_identity_op(int op, ConstVal c) {
    if (!c.is_const) return 0;
    if (c.int_val == 0 && (op == ALIR_OP_ADD || op == ALIR_OP_SUB || op == ALIR_OP_OR || op == ALIR_OP_XOR)) return 1;
    if (c.int_val == 1 && (op == ALIR_OP_MUL || op == ALIR_OP_DIV || op == ALIR_OP_MOD)) return 1;
    if (c.int_val == -1 && (op == ALIR_OP_AND)) return 1;
    if (c.int_val == 0 && (op == ALIR_OP_MUL)) return 1;
    return 0;
}

static int is_self_cancel_op(int op) {
    return (op == ALIR_OP_SUB || op == ALIR_OP_XOR);
}

static void remove_instruction(AlirBlock *block, AlirInst *prev, AlirInst *inst) {
    if (prev) {
        prev->next = inst->next;
    } else {
        block->head = inst->next;
    }
    if (block->tail == inst) {
        block->tail = prev;
    }
}

static void constant_propagate_function(AlirModule *module, AlirFunction *func) {
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
            
            if (!removed) {
                prev = i;
            }
            i = next;
        }
        b = b->next;
    }
}

static int value_is_used_somewhere(AlirFunction *func, AlirValue *val) {
    if (!val || val->kind != ALIR_VAL_TEMP) return 1;
    
    AlirBlock *b = func->blocks;
    while (b) {
        AlirInst *i = b->head;
        while (i) {
            if (i->op1 == val) return 1;
            if (i->op2 == val) return 1;
            if (i->dest == val) {
                if (i->op == ALIR_OP_STORE) return 1;
                i = i->next;
                continue;
            }
            for (int j = 0; j < i->arg_count; j++) {
                if (i->args[j] == val) return 1;
            }
            i = i->next;
        }
        b = b->next;
    }
    return 0;
}

static void remove_dead_stores_function(AlirModule *module, AlirFunction *func) {
    (void)module;
    if (!func || !func->blocks) return;
    
    AlirBlock *b = func->blocks;
    while (b) {
        AlirInst **pip = &b->head;
        while (*pip) {
            AlirInst *inst = *pip;
            if (inst->op == ALIR_OP_STORE && inst->op2 && inst->op2->kind == ALIR_VAL_TEMP) {
                if (!value_is_used_somewhere(func, inst->op2)) {
                    *pip = inst->next;
                    continue;
                }
            }
            pip = &inst->next;
        }
        b = b->next;
    }
}

static int is_temp_used_except_in_load(AlirFunction *func, AlirValue *temp, AlirInst *exclude_load) {
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

static void propagate_param_copies_function(AlirModule *module, AlirFunction *func) {
    (void)module;
    if (!func || !func->blocks) return;
    
    AlirBlock *entry = func->blocks;
    if (!entry) return;
    
    AlirInst *i = entry->head;
    while (i) {
        if (i->op == ALIR_OP_ALLOCA && i->dest) {
            AlirInst *next = i->next;
            if (next && next->op == ALIR_OP_STORE && next->op1 && next->op1->kind == ALIR_VAL_VAR && next->op2 == i->dest) {
                AlirInst *next2 = next->next;
                if (next2 && next2->op == ALIR_OP_LOAD && next2->op1 == i->dest) {
                    if (!is_temp_used_except_in_load(func, i->dest, next2)) {
                        char *param_name = strdup(next->op1->val.str_val);
                        
                        next2->op = 0;
                        next2->dest->kind = ALIR_VAL_VAR;
                        next2->dest->val.str_val = param_name;
                        next2->op1 = NULL;
                        next2->op2 = NULL;
                        
                        AlirInst *to_remove[2];
                        to_remove[0] = i;
                        to_remove[1] = next;
                        
                        for (int r = 0; r < 2; r++) {
                            AlirInst *inst = to_remove[r];
                            AlirInst *prev = NULL;
                            AlirInst *curr = entry->head;
                            while (curr && curr != inst) {
                                prev = curr;
                                curr = curr->next;
                            }
                            if (curr == inst) {
                                remove_instruction(entry, prev, inst);
                            }
                        }
                    }
                }
            }
        }
        i = i->next;
    }
}

void optlir_local_optimize(AlirModule *module) {
    if (!module) return;
    
    AlirFunction *func = module->functions;
    while (func) {
        if (!func->is_extern) {
            constant_propagate_function(module, func);
            remove_dead_stores_function(module, func);
            propagate_param_copies_function(module, func);
        }
        func = func->next;
    }
}
