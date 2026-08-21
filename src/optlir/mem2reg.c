/**
 * @file mem2reg.c
 * @brief Memory-to-register promotion pass for ALIR.
 */
#include "optlir.h"
#include "common/arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief Promote stack allocations to registers where possible (local mem2reg pass).
 * @param module The ALIR module.
 */
void optlir_mem2reg_local(AlirModule *module) {
    AlirFunction *func = module->functions;
    while(func) {
        HashMap repl_map;
        hashmap_init(&repl_map, module->compiler_ctx->arena, 256);
        
        AlirBlock *b = func->blocks;
        while(b) {
            HashMap store_map;
            hashmap_init(&store_map, module->compiler_ctx->arena, 64);
            
            AlirInst *inst = b->head;
            while(inst) {
                if (inst->op == ALIR_OP_STORE && inst->op2 && inst->op2->kind == ALIR_VAL_TEMP && inst->op1) {
                    char key[32];
                    snprintf(key, sizeof(key), "%d", inst->op2->temp_id);
                    hashmap_put(&store_map, key, inst->op1);
                }
                else if (inst->op == ALIR_OP_LOAD && inst->op1 && inst->op1->kind == ALIR_VAL_TEMP && inst->dest && inst->dest->kind == ALIR_VAL_TEMP) {
                    char key[32];
                    snprintf(key, sizeof(key), "%d", inst->op1->temp_id);
                    AlirValue *val = hashmap_get(&store_map, key);
                    if (val) {
                        char dest_key[32];
                        snprintf(dest_key, sizeof(dest_key), "%d", inst->dest->temp_id);
                        hashmap_put(&repl_map, dest_key, val);
                        
                        inst->op = ALIR_OP_FREE_STACK; // Make it a NOP
                        inst->dest = NULL;
                        inst->op1 = NULL;
                    }
                }
                inst = inst->next;
            }
            hashmap_free(&store_map);
            b = b->next;
        }
        
        b = func->blocks;
        while(b) {
            AlirInst *inst = b->head;
            while(inst) {
                if (inst->op1 && inst->op1->kind == ALIR_VAL_TEMP) {
                    char key[32]; snprintf(key, sizeof(key), "%d", inst->op1->temp_id);
                    AlirValue *repl = hashmap_get(&repl_map, key);
                    while(repl && repl->kind == ALIR_VAL_TEMP) {
                        char k2[32]; snprintf(k2, sizeof(k2), "%d", repl->temp_id);
                        AlirValue *r2 = hashmap_get(&repl_map, k2);
                        if (r2) repl = r2; else break;
                    }
                    if (repl) inst->op1 = repl;
                }
                if (inst->op2 && inst->op2->kind == ALIR_VAL_TEMP) {
                    char key[32]; snprintf(key, sizeof(key), "%d", inst->op2->temp_id);
                    AlirValue *repl = hashmap_get(&repl_map, key);
                    while(repl && repl->kind == ALIR_VAL_TEMP) {
                        char k2[32]; snprintf(k2, sizeof(k2), "%d", repl->temp_id);
                        AlirValue *r2 = hashmap_get(&repl_map, k2);
                        if (r2) repl = r2; else break;
                    }
                    if (repl) inst->op2 = repl;
                }
                for(int i=0; i<inst->arg_count; i++) {
                    if (inst->args[i] && inst->args[i]->kind == ALIR_VAL_TEMP) {
                        char key[32]; snprintf(key, sizeof(key), "%d", inst->args[i]->temp_id);
                        AlirValue *repl = hashmap_get(&repl_map, key);
                        while(repl && repl->kind == ALIR_VAL_TEMP) {
                            char k2[32]; snprintf(k2, sizeof(k2), "%d", repl->temp_id);
                            AlirValue *r2 = hashmap_get(&repl_map, k2);
                            if (r2) repl = r2; else break;
                        }
                        if (repl) inst->args[i] = repl;
                    }
                }
                inst = inst->next;
            }
            b = b->next;
        }
        hashmap_free(&repl_map);
        func = func->next;
    }
}

/**
 * @brief Dead-code eliminate unused alloca instructions.
 * @param module The ALIR module.
 */
void optlir_dce_allocs(AlirModule *module) {
    AlirFunction *func = module->functions;
    while(func) {
        HashMap used_map;
        hashmap_init(&used_map, module->compiler_ctx->arena, 256);
        
        HashMap alloc_map;
        hashmap_init(&alloc_map, module->compiler_ctx->arena, 256);
        
        AlirBlock *b = func->blocks;
        while(b) {
            AlirInst *inst = b->head;
            while(inst) {
                if (inst->op == ALIR_OP_ALLOCA && inst->dest && inst->dest->kind == ALIR_VAL_TEMP) {
                    char key[32]; snprintf(key, sizeof(key), "%d", inst->dest->temp_id);
                    hashmap_put(&alloc_map, key, (void*)1);
                }
                if (inst->op != ALIR_OP_STORE && inst->op != ALIR_OP_ALLOCA) {
                    if (inst->op1 && inst->op1->kind == ALIR_VAL_TEMP) {
                        char key[32]; snprintf(key, sizeof(key), "%d", inst->op1->temp_id);
                        hashmap_put(&used_map, key, (void*)1);
                    }
                    if (inst->op2 && inst->op2->kind == ALIR_VAL_TEMP) {
                        char key[32]; snprintf(key, sizeof(key), "%d", inst->op2->temp_id);
                        hashmap_put(&used_map, key, (void*)1);
                    }
                    for(int i=0; i<inst->arg_count; i++) {
                        if (inst->args[i] && inst->args[i]->kind == ALIR_VAL_TEMP) {
                            char key[32]; snprintf(key, sizeof(key), "%d", inst->args[i]->temp_id);
                            hashmap_put(&used_map, key, (void*)1);
                        }
                    }
                }
                inst = inst->next;
            }
            b = b->next;
        }
        
        b = func->blocks;
        while(b) {
            AlirInst *inst = b->head;
            while(inst) {
                if (inst->op == ALIR_OP_ALLOCA && inst->dest && inst->dest->kind == ALIR_VAL_TEMP) {
                    char key[32]; snprintf(key, sizeof(key), "%d", inst->dest->temp_id);
                    if (!hashmap_get(&used_map, key)) {
                        inst->op = ALIR_OP_FREE_STACK; // NOP
                        inst->dest = NULL;
                    }
                }
                else if (inst->op == ALIR_OP_STORE && inst->op2 && inst->op2->kind == ALIR_VAL_TEMP) {
                    char key[32]; snprintf(key, sizeof(key), "%d", inst->op2->temp_id);
                    // Only delete the store if the target is an unused ALLOCA.
                    // If it is a GETPTR or anything else, we don't know if the base is used,
                    // so it is unsafe to delete it blindly.
                    if (hashmap_get(&alloc_map, key) && !hashmap_get(&used_map, key)) {
                        inst->op = ALIR_OP_FREE_STACK; // NOP
                        inst->op1 = NULL;
                        inst->op2 = NULL;
                    }
                }
                inst = inst->next;
            }
            b = b->next;
        }
        hashmap_free(&used_map);
        hashmap_free(&alloc_map);
        func = func->next;
    }
}
