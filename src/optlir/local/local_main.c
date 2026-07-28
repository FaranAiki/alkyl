#include "optlir.h"
#include "optlir/local.h"
#include "optlir/local_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void optlir_local_optimize(AlirModule *module) {
    if (!module) return;
    
    AlirFunction *func = module->functions;
    while (func) {
        if (!func->is_extern) {
            constant_propagate_function(module, func);
            fold_branches_function(module, func);
            merge_entry_jump_function(module, func);
            remove_unreachable_blocks_function(module, func);
            remove_dead_stores_function(module, func);
            propagate_param_copies_function(module, func);
            eval_pure_call_function(module, func);
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