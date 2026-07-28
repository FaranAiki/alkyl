#include "optlir.h"
#include "optlir/local.h"
#include "optlir/local_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void free_edges(BlockEdge *e) {
    while (e) {
        BlockEdge *next = e->next;
        free(e);
        e = next;
    }
}

void redirect_label_to(AlirModule *module, AlirBlock *b, const char *old_label, const char *new_label) {
    if (!b || !old_label || !new_label || strcmp(old_label, new_label) == 0) return;
    AlirInst *i = b->head;
    while (i) {
        if (i->op == ALIR_OP_JUMP && i->op1 && i->op1->kind == ALIR_VAL_LABEL &&
            strcmp(i->op1->val.str_val, old_label) == 0) {
            i->op1 = alir_val_label(module, new_label);
        } else if (i->op == ALIR_OP_CONDI) {
            if (i->op2 && i->op2->kind == ALIR_VAL_LABEL &&
                strcmp(i->op2->val.str_val, old_label) == 0) {
                i->op2 = alir_val_label(module, new_label);
            }
            if (i->arg_count > 0 && i->args[0] && i->args[0]->kind == ALIR_VAL_LABEL &&
                strcmp(i->args[0]->val.str_val, old_label) == 0) {
                i->args[0] = alir_val_label(module, new_label);
            }
        }
        i = i->next;
    }
}

void redirect_label_in_all_blocks(AlirModule *module, AlirFunction *func, const char *old_label, const char *new_label) {
    AlirBlock *b = func->blocks;
    while (b) {
        redirect_label_to(module, b, old_label, new_label);
        b = b->next;
    }
}

void build_pred_succ(AlirFunction *func) {
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
                BlockEdge *se = malloc(sizeof(BlockEdge));
                se->block = t;
                se->next = b->succ;
                b->succ = se;
                BlockEdge *pe = malloc(sizeof(BlockEdge));
                pe->block = b;
                pe->next = t->pred;
                t->pred = pe;
            }
            i = i->next;
        }
        b = b->next;
    }
}

int merge_entry_jump_function(AlirModule *module, AlirFunction *func) {
    if (!func || !func->blocks) return 0;

    {
        AlirBlock *b = func->blocks;
        while (b) {
            free_edges(b->pred);
            free_edges(b->succ);
            b->pred = NULL;
            b->succ = NULL;
            b = b->next;
        }
        build_pred_succ(func);
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

    if (strcmp(entry->label, target_label) != 0) {
        redirect_label_in_all_blocks(module, func, entry->label, target_label);
    }

    free_edges(entry->pred);
    free_edges(entry->succ);

    return 1;
}