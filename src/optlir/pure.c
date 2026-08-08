#include "optlir.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    long long int_val;
    double double_val;
    int is_const;
    int is_float;
} EvalVal;

static EvalVal get_val(AlirValue *v) {
    EvalVal res = {0};
    if (!v) return res;
    if (v->kind == ALIR_VAL_CONST) {
        res.is_const = 1;
        res.is_float = (v->type.base == TYPE_SINGLE || v->type.base == TYPE_DOUBLE);
        if (res.is_float) {
            if (v->type.base == TYPE_SINGLE) res.double_val = v->val.single_val;
            else res.double_val = v->val.double_val;
        } else {
            res.int_val = v->val.long_long_val;
        }
    }
    return res;
}

static EvalVal eval_pure_function_impl(AlirModule *module, AlirFunction *func, AlirValue **args, int arg_count, VarType ret_type) {
    (void)module;
    EvalVal res = {0};

    if (arg_count != func->param_count) return res;

    AlirParam *p = func->params;
    EvalVal arg_vals[16];
    for (int i = 0; i < arg_count && i < 16; i++) {
        arg_vals[i] = get_val(args[i]);
        if (!arg_vals[i].is_const) return res;
        p = p ? p->next : NULL;
    }

    if (streq_lit(func->name, "abs") && arg_count == 1) {
        res = arg_vals[0];
        if (res.is_float) {
            if (res.double_val < 0) res.double_val = -res.double_val;
        } else {
            if (res.int_val < 0) res.int_val = -res.int_val;
        }
        res.is_const = 1;
        return res;
    }

    AlirBlock *entry = func->blocks;
    if (!entry) return res;

    AlirInst *i = entry->head;
    while (i) {
        if (i->op == ALIR_OP_RET && i->op1) {
            EvalVal v = get_val(i->op1);
            if (v.is_const) {
                res = v;
                if (ret_type.base == TYPE_SINGLE && res.is_float) {
                    res.double_val = (float)res.double_val;
                } else if (ret_type.base == TYPE_DOUBLE && !res.is_float) {
                    res.double_val = (double)res.int_val;
                    res.is_float = 1;
                } else if (ret_type.base != TYPE_SINGLE && ret_type.base != TYPE_DOUBLE && res.is_float) {
                    res.int_val = (long long)res.double_val;
                    res.is_float = 0;
                }
                res.is_const = 1;
                return res;
            }
        }
        i = i->next;
    }

    return res;
}

ConstVal eval_pure_function(AlirModule *module, AlirFunction *func, AlirValue **args, int arg_count, VarType ret_type) {
    EvalVal ev = eval_pure_function_impl(module, func, args, arg_count, ret_type);
    ConstVal res = {0};
    res.is_const = ev.is_const;
    res.is_float = ev.is_float;
    res.int_val = ev.int_val;
    res.double_val = ev.double_val;
    return res;
}
