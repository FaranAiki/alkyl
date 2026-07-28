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

static EvalVal eval_binary(int op, EvalVal l, EvalVal r, int is_float) {
    EvalVal res = {0};
    if (!l.is_const || !r.is_const) return res;

    if (is_float) {
        double d1 = l.is_float ? l.double_val : (double)l.int_val;
        double d2 = r.is_float ? r.double_val : (double)r.int_val;
        res.is_float = 1;
        switch (op) {
            case ALIR_OP_FADD: case ALIR_OP_ADD: res.double_val = d1 + d2; break;
            case ALIR_OP_FSUB: case ALIR_OP_SUB: res.double_val = d1 - d2; break;
            case ALIR_OP_FMUL: case ALIR_OP_MUL: res.double_val = d1 * d2; break;
            case ALIR_OP_FDIV: case ALIR_OP_DIV: res.double_val = d2 != 0 ? d1 / d2 : 0; break;
            case ALIR_OP_LT: res.int_val = d1 < d2; break;
            case ALIR_OP_GT: res.int_val = d1 > d2; break;
            case ALIR_OP_LTE: res.int_val = d1 <= d2; break;
            case ALIR_OP_GTE: res.int_val = d1 >= d2; break;
            case ALIR_OP_EQ: res.int_val = d1 == d2; break;
            case ALIR_OP_NEQ: res.int_val = d1 != d2; break;
            default: return res;
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
        case ALIR_OP_ROTR: {
            int shift = (int)(v2 & 63);
            if (shift == 0) {
                res.int_val = v1;
            } else {
                res.int_val = (v1 >> shift) | (v1 << (64 - shift));
            }
            break;
        }
        case ALIR_OP_ROTL: {
            int shift = (int)(v2 & 63);
            if (shift == 0) {
                res.int_val = v1;
            } else {
                res.int_val = (v1 << shift) | (v1 >> (64 - shift));
            }
            break;
        }
        case ALIR_OP_LT:  res.int_val = v1 < v2; break;
        case ALIR_OP_GT:  res.int_val = v1 > v2; break;
        case ALIR_OP_LTE: res.int_val = v1 <= v2; break;
        case ALIR_OP_GTE: res.int_val = v1 >= v2; break;
        case ALIR_OP_EQ:  res.int_val = v1 == v2; break;
        case ALIR_OP_NEQ: res.int_val = v1 != v2; break;
        default: return res;
    }
    res.is_const = 1;
    return res;
}

static EvalVal eval_unary(int op, EvalVal v, int is_float) {
    EvalVal res = {0};
    if (!v.is_const) return res;

    if (is_float) {
        res.is_float = 1;
        switch (op) {
            case ALIR_OP_NOT: res.int_val = ~(long long)v.double_val; break;
            default: return res;
        }
        res.is_const = 1;
        return res;
    }

    switch (op) {
        case ALIR_OP_NOT: res.int_val = ~v.int_val; break;
        default: return res;
    }
    res.is_const = 1;
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

    int is_float = (ret_type.base == TYPE_SINGLE || ret_type.base == TYPE_DOUBLE);

    if (strcmp(func->name, "abs") == 0 && arg_count == 1) {
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
