#include "optlir.h"
#include "optlir/local.h"
#include "optlir/local_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

void propagate_param_copies_function(AlirModule *module, AlirFunction *func) {
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