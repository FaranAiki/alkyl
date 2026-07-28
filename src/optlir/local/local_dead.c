#include "optlir.h"
#include "optlir/local.h"
#include "optlir/local_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void remove_instruction(AlirBlock *block, AlirInst *prev, AlirInst *inst) {
    if (prev) {
        prev->next = inst->next;
    } else {
        block->head = inst->next;
    }
    if (block->tail == inst) {
        block->tail = prev;
    }
}

int value_is_used_somewhere(AlirFunction *func, AlirValue *val) {
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

void remove_dead_stores_function(AlirModule *module, AlirFunction *func) {
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