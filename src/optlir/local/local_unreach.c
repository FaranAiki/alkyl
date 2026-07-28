#include "optlir.h"
#include "optlir/local.h"
#include "optlir/local_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

void remove_unreachable_blocks_function(AlirModule *module, AlirFunction *func) {
    if (!func || !func->blocks) return;
    
    BlockSet reachable;
    mark_reachable_blocks(func, &reachable);
    
    if (reachable.count == func->block_count) {
        block_set_free(&reachable);
        return;
    }
    
    AlirBlock **new_blocks = calloc(reachable.count, sizeof(AlirBlock*));
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
    free(new_blocks);
}