#include "optlir.h"
#include "optlir/local.h"
#include "common/arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

ConstVal eval_pure_function(AlirModule *module, AlirFunction *func, AlirValue **args, int arg_count, VarType ret_type);

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
        case ALIR_OP_NOT: res.int_val = type.base == TYPE_BOOL ? !v.int_val : ~v.int_val; break;
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

static void free_edges(BlockEdge *e) {
    (void)e;
}

static void redirect_label_to(AlirModule *module, AlirBlock *b, const char *old_label, const char *new_label) {
    if (!b || !old_label || !new_label || streq(old_label, new_label)) return;
    AlirInst *i = b->head;
    while (i) {
        if (i->op == ALIR_OP_JUMP && i->op1 && i->op1->kind == ALIR_VAL_LABEL &&
            streq(i->op1->val.str_val, old_label)) {
            i->op1 = alir_val_label(module, new_label);
        } else if (i->op == ALIR_OP_CONDI) {
            if (i->op2 && i->op2->kind == ALIR_VAL_LABEL &&
                streq(i->op2->val.str_val, old_label)) {
                i->op2 = alir_val_label(module, new_label);
            }
            if (i->arg_count > 0 && i->args[0] && i->args[0]->kind == ALIR_VAL_LABEL &&
                streq(i->args[0]->val.str_val, old_label)) {
                i->args[0] = alir_val_label(module, new_label);
            }
        }
        i = i->next;
    }
}

static void redirect_label_in_all_blocks(AlirModule *module, AlirFunction *func, const char *old_label, const char *new_label) {
    AlirBlock *b = func->blocks;
    while (b) {
        redirect_label_to(module, b, old_label, new_label);
        b = b->next;
    }
}

static AlirBlock* find_block_by_label(AlirFunction *func, const char *label);

static void build_pred_succ(AlirFunction *func, Arena *arena) {
    if (!func || !func->blocks) return;
    AlirBlock *b = func->blocks;
    while (b) {
        b->pred = NULL;
        b->succ = NULL;
        b = b->next;
    }
    b = func->blocks;
    while (b) {
        AlirInst *i = b->head;
        while (i) {
            AlirBlock *t = NULL;
            if (i->op == ALIR_OP_JUMP && i->op1 && i->op1->kind == ALIR_VAL_LABEL) {
                t = find_block_by_label(func, i->op1->val.str_val);
            } else if (i->op == ALIR_OP_CONDI) {
                if (i->op2 && i->op2->kind == ALIR_VAL_LABEL) {
                    t = find_block_by_label(func, i->op2->val.str_val);
                }
                if (!t && i->arg_count > 0 && i->args[0] && i->args[0]->kind == ALIR_VAL_LABEL) {
                    t = find_block_by_label(func, i->args[0]->val.str_val);
                }
            }
            if (t && t != b) {
                BlockEdge *se = arena_alloc_type(arena, BlockEdge);
                se->block = t;
                se->next = b->succ;
                b->succ = se;
                BlockEdge *pe = arena_alloc_type(arena, BlockEdge);
                pe->block = b;
                pe->next = t->pred;
                t->pred = pe;
            }
            i = i->next;
        }
        b = b->next;
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

static void fold_branches_function(AlirModule *module, AlirFunction *func) {
    if (!func || !func->blocks) return;

    AlirBlock *b = func->blocks;
    while (b) {
        AlirInst *i = b->head;
        while (i) {
            if (i->op == ALIR_OP_CONDI && i->op1) {
                ConstVal cond = get_const_for_value(i->op1);
                if (cond.is_const) {
                    const char *target_label = NULL;
                    if (cond.int_val != 0 && i->op2) {
                        target_label = i->op2->val.str_val;
                    } else if (cond.int_val == 0 && i->arg_count > 0 && i->args[0]) {
                        target_label = i->args[0]->val.str_val;
                    }
                    if (target_label) {
                        i->op = ALIR_OP_JUMP;
                        i->op1 = alir_val_label(module, target_label);
                        i->op2 = NULL;
                        if (i->args) {
                            i->args = NULL;
                            i->arg_count = 0;
                        }
                    }
                }
            }
            i = i->next;
        }
        b = b->next;
    }
}

typedef struct BlockSet {
    AlirBlock **blocks;
    int count;
    int capacity;
} BlockSet;

static void block_set_init(BlockSet *set, int capacity, Arena *arena) {
    set->blocks = arena_alloc(arena, sizeof(AlirBlock *) * capacity);
    memset(set->blocks, 0, sizeof(AlirBlock *) * capacity);
    set->count = 0;
    set->capacity = capacity;
}

static void block_set_add(BlockSet *set, AlirBlock *b, Arena *arena) {
    if (!b) return;
    for (int i = 0; i < set->count; i++) {
        if (set->blocks[i] == b) return;
    }
    if (set->count >= set->capacity) {
        int new_cap = set->capacity * 2;
        AlirBlock **new_blocks = arena_alloc(arena, sizeof(AlirBlock *) * new_cap);
        memset(new_blocks + set->count, 0, (new_cap - set->count) * sizeof(AlirBlock *));
        if (set->blocks) {
            memcpy(new_blocks, set->blocks, set->count * sizeof(AlirBlock *));
        }
        set->blocks = new_blocks;
        set->capacity = new_cap;
    }
    set->blocks[set->count++] = b;
}

static int block_set_has(BlockSet *set, AlirBlock *b) {
    if (!b) return 0;
    for (int i = 0; i < set->count; i++) {
        if (set->blocks[i] == b) return 1;
    }
    return 0;
}

static void block_set_free(BlockSet *set) {
    set->blocks = NULL;
    set->count = 0;
    set->capacity = 0;
}

static AlirBlock* find_block_by_label(AlirFunction *func, const char *label) {
    if (!func || !label) return NULL;
    AlirBlock *b = func->blocks;
    while (b) {
        if (b->label && streq(b->label, label)) return b;
        b = b->next;
    }
    return NULL;
}

static void mark_reachable_blocks(AlirFunction *func, BlockSet *reachable, Arena *arena) {
    if (!func || !func->blocks) return;

    block_set_init(reachable, func->block_count > 0 ? func->block_count : 8, arena);

    AlirBlock *entry = func->blocks;
    if (!entry) return;

    AlirBlock **stack = arena_alloc(arena, sizeof(AlirBlock *) * func->block_count);
    int stack_top = 0;
    stack[stack_top++] = entry;
    block_set_add(reachable, entry, arena);

    while (stack_top > 0) {
        AlirBlock *b = stack[--stack_top];
        AlirInst *i = b->head;
        while (i) {
            if (i->op == ALIR_OP_JUMP && i->op1 && i->op1->kind == ALIR_VAL_LABEL) {
                AlirBlock *target = find_block_by_label(func, i->op1->val.str_val);
                if (target && !block_set_has(reachable, target)) {
                    block_set_add(reachable, target, arena);
                    stack[stack_top++] = target;
                }
            } else if (i->op == ALIR_OP_CONDI && i->op1 && i->op2 && i->op2->kind == ALIR_VAL_LABEL) {
                AlirBlock *target = find_block_by_label(func, i->op2->val.str_val);
                if (target && !block_set_has(reachable, target)) {
                    block_set_add(reachable, target, arena);
                    stack[stack_top++] = target;
                }
                if (i->arg_count > 0 && i->args[0] && i->args[0]->kind == ALIR_VAL_LABEL) {
                    target = find_block_by_label(func, i->args[0]->val.str_val);
                    if (target && !block_set_has(reachable, target)) {
                        block_set_add(reachable, target, arena);
                        stack[stack_top++] = target;
                    }
                }
            }
            i = i->next;
        }
    }
}

static void remove_unreachable_blocks_function(AlirModule *module, AlirFunction *func) {
    if (!func || !func->blocks) return;

    Arena *arena = module->compiler_ctx ? module->compiler_ctx->arena : NULL;
    BlockSet reachable;
    mark_reachable_blocks(func, &reachable, arena);

    if (reachable.count == func->block_count) {
        block_set_free(&reachable);
        return;
    }

    AlirBlock **new_blocks = arena_alloc(arena, sizeof(AlirBlock *) * reachable.count);
    memset(new_blocks, 0, sizeof(AlirBlock *) * reachable.count);
    int new_count = 0;

    AlirBlock *prev = NULL;
    AlirBlock *b = func->blocks;
    while (b) {
        if (block_set_has(&reachable, b)) {
            new_blocks[new_count++] = b;
            prev = b;
            b = b->next;
        } else {
            AlirBlock *to_remove = b;
            b = b->next;
            if (prev) prev->next = b;
            else func->blocks = b;
            func->block_count--;
            free_edges(to_remove->pred);
            free_edges(to_remove->succ);
        }
    }

    block_set_free(&reachable);
}

static int merge_entry_jump_function(AlirModule *module, AlirFunction *func) {
    if (!func || !func->blocks) return 0;

    Arena *arena = module->compiler_ctx ? module->compiler_ctx->arena : NULL;
    {
        AlirBlock *b = func->blocks;
        while (b) {
            free_edges(b->pred);
            free_edges(b->succ);
            b->pred = NULL;
            b->succ = NULL;
            b = b->next;
        }
        build_pred_succ(func, arena);
    }

    AlirBlock *entry = func->blocks;
    if (!entry || !entry->head) return 0;

    AlirInst *i = entry->head;
    if (i->op != ALIR_OP_JUMP || i->op1 == NULL || i->op1->kind != ALIR_VAL_LABEL) return 0;
    if (i->next != NULL) return 0;

    const char *target_label = i->op1->val.str_val;
    AlirBlock *target = find_block_by_label(func, target_label);
    if (!target || target == entry) return 0;

    int pred_count = 0;
    for (BlockEdge *e = target->pred; e; e = e->next) pred_count++;
    if (pred_count != 1) return 0;

    if (func->blocks == entry) {
        func->blocks = entry->next;
    } else {
        AlirBlock *prev_entry = func->blocks;
        while (prev_entry && prev_entry->next != entry) prev_entry = prev_entry->next;
        if (prev_entry) prev_entry->next = entry->next;
    }
    func->block_count--;

    if (!streq(entry->label, target_label)) {
        redirect_label_in_all_blocks(module, func, entry->label, target_label);
    }

    free_edges(entry->pred);
    free_edges(entry->succ);

    return 1;
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
    Arena *arena = module->compiler_ctx ? module->compiler_ctx->arena : NULL;
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
                        char *param_name = arena_strdup(arena, next->op1->val.str_val);

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

static int all_args_const(AlirInst *inst) {
    if (!inst || inst->arg_count <= 0) return 1;
    for (int i = 0; i < inst->arg_count; i++) {
        if (!inst->args[i]) return 0;
        if (inst->args[i]->kind != ALIR_VAL_CONST) return 0;
    }
    return 1;
}

static void eval_pure_call_function(AlirModule *module, AlirFunction *func) {
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
                    if (streq(callee->name, i->op1->val.str_val) && callee->is_pure && !callee->is_extern && callee->block_count > 0) {
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

static void forward_empty_blocks_function(AlirModule *module, AlirFunction *func) {
    if (!func || !func->blocks) return;
    
    int changed;
    do {
        changed = 0;
        AlirBlock *b = func->blocks;
        while (b) {
            // We skip func->blocks (entry block) because we can't redirect implicit entry jumps.
            if (b != func->blocks && b->head && b->head->op == ALIR_OP_JUMP && b->head->next == NULL) {
                if (b->head->op1 && b->head->op1->kind == ALIR_VAL_LABEL) {
                    const char *target_label = b->head->op1->val.str_val;
                    if (!streq(b->label, target_label)) {
                        redirect_label_in_all_blocks(module, func, b->label, target_label);
                        // Prevent infinite loop by making it jump to itself, it will be removed as unreachable
                        b->head->op1->val.str_val = b->label;
                        changed = 1;
                    }
                }
            }
            b = b->next;
        }
    } while (changed);
}

void optlir_optimize(AlirModule *module, int opt_level) {
    if (!module || opt_level <= 0) return;

    optlir_mem2reg_local(module);

    // Clean up NOPs generated by mem2reg before further optimization passes
    AlirFunction *f = module->functions;
    while (f) {
        AlirBlock *b = f->blocks;
        while (b) {
            AlirInst *prev = NULL;
            AlirInst *i = b->head;
            while (i) {
                if (i->op == ALIR_OP_FREE_STACK) {
                    AlirInst *to_remove = i;
                    i = i->next;
                    if (prev) prev->next = i;
                    else b->head = i;
                    if (b->tail == to_remove) b->tail = prev;
                } else {
                    prev = i;
                    i = i->next;
                }
            }
            b = b->next;
        }
        f = f->next;
    }
    int max_iters = (opt_level >= 3) ? 5 : 1;
    for (int iter = 0; iter < max_iters; iter++) {
        AlirFunction *func = module->functions;
        while (func) {
            if (!func->is_extern) {
                if (opt_level >= 1) {
                    remove_unreachable_blocks_function(module, func);
                    forward_empty_blocks_function(module, func);
                }
                if (opt_level >= 2) {
                    constant_propagate_function(module, func);
                    fold_branches_function(module, func);
                    merge_entry_jump_function(module, func);
                    remove_dead_stores_function(module, func);
                    propagate_param_copies_function(module, func);
                }
                if (opt_level >= 3) {
                    eval_pure_call_function(module, func);
                }
            }

            {
                AlirBlock *b = func->blocks;
                while (b) {
                    free_edges(b->pred);
                    free_edges(b->succ);
                    b->pred = NULL;
                    b->succ = NULL;
                    b = b->next;
                }
            }
            func = func->next;
        }
    }
    optlir_dce_allocs(module);
}
