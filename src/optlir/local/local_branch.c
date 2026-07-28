#include "optlir.h"
#include "optlir/local.h"
#include "optlir/local_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void fold_branches_function(AlirModule *module, AlirFunction *func) {
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