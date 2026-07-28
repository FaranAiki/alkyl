#include "optlir.h"
#include "optlir/local.h"
#include "optlir/local_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>



void block_set_init(BlockSet *set, int capacity) {
    set->blocks = calloc(capacity, sizeof(AlirBlock*));
    set->count = 0;
    set->capacity = capacity;
}

void block_set_add(BlockSet *set, AlirBlock *b) {
    if (!b) return;
    for (int i = 0; i < set->count; i++) {
        if (set->blocks[i] == b) return;
    }
    if (set->count >= set->capacity) {
        set->capacity *= 2;
        set->blocks = realloc(set->blocks, set->capacity * sizeof(AlirBlock*));
        memset(set->blocks + set->count, 0, (set->capacity - set->count) * sizeof(AlirBlock*));
    }
    set->blocks[set->count++] = b;
}

int block_set_has(BlockSet *set, AlirBlock *b) {
    if (!b) return 0;
    for (int i = 0; i < set->count; i++) {
        if (set->blocks[i] == b) return 1;
    }
    return 0;
}

void block_set_free(BlockSet *set) {
    free(set->blocks);
    set->blocks = NULL;
    set->count = 0;
    set->capacity = 0;
}

AlirBlock* find_block_by_label(AlirFunction *func, const char *label) {
    if (!func || !label) return NULL;
    AlirBlock *b = func->blocks;
    while (b) {
        if (b->label && strcmp(b->label, label) == 0) return b;
        b = b->next;
    }
    return NULL;
}

void mark_reachable_blocks(AlirFunction *func, BlockSet *reachable) {
    if (!func || !func->blocks) return;
    
    block_set_init(reachable, func->block_count > 0 ? func->block_count : 8);
    
    AlirBlock *entry = func->blocks;
    if (!entry) return;
    
    AlirBlock **stack = malloc(sizeof(AlirBlock*) * func->block_count);
    int stack_top = 0;
    stack[stack_top++] = entry;
    block_set_add(reachable, entry);
    
    while (stack_top > 0) {
        AlirBlock *b = stack[--stack_top];
        AlirInst *i = b->head;
        while (i) {
            if (i->op == ALIR_OP_JUMP && i->op1 && i->op1->kind == ALIR_VAL_LABEL) {
                AlirBlock *target = find_block_by_label(func, i->op1->val.str_val);
                if (target && !block_set_has(reachable, target)) {
                    block_set_add(reachable, target);
                    stack[stack_top++] = target;
                }
            } else if (i->op == ALIR_OP_CONDI && i->op1 && i->op2 && i->op2->kind == ALIR_VAL_LABEL) {
                AlirBlock *target = find_block_by_label(func, i->op2->val.str_val);
                if (target && !block_set_has(reachable, target)) {
                    block_set_add(reachable, target);
                    stack[stack_top++] = target;
                }
                if (i->arg_count > 0 && i->args[0] && i->args[0]->kind == ALIR_VAL_LABEL) {
                    target = find_block_by_label(func, i->args[0]->val.str_val);
                    if (target && !block_set_has(reachable, target)) {
                        block_set_add(reachable, target);
                        stack[stack_top++] = target;
                    }
                }
            }
            i = i->next;
        }
    }
    
    free(stack);
}

