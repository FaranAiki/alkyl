#include "meta/vm_internal.h"
#include <string.h>
#include "common/arena.h"

void vm_eval_mem(VMContext *ctx, AlirInst *inst) {
    switch(inst->op) {
case ALIR_OP_ALLOCA: {
                    if (inst->dest) {
                        ctx->registers[inst->dest->temp_id].as.ptr_val = arena_alloc(ctx->vm->arena, 1024); 
                    }
                    break;
                }
                    break;
case ALIR_OP_STORE: {
                    if (inst->op1 && inst->op2) { // op1 = value, op2 = ptr
                        long long val = 0;
                        if (inst->op1->kind == ALIR_VAL_CONST) val = inst->op1->val.long_long_val;
                        else if (inst->op1->kind == ALIR_VAL_TEMP) val = ctx->registers[inst->op1->temp_id].as.int_val;
                        else if (inst->op1->kind == ALIR_VAL_VAR) val = meta_vm_resolve_var(inst->op1, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                        else if (inst->op1->kind == ALIR_VAL_GLOBAL && ctx->module) {
                            AlirGlobal *g = ctx->module->globals;
                            while(g) {
                                if (strcmp(g->name, inst->op1->val.str_val) == 0) {
                                    val = (long long)(intptr_t)g->string_content;
                                    break;
                                }
                                g = g->next;
                            }
                        }
                        
                        void *ptr = NULL;
                        if (inst->op2->kind == ALIR_VAL_TEMP) ptr = ctx->registers[inst->op2->temp_id].as.ptr_val;
                        else if (inst->op2->kind == ALIR_VAL_GLOBAL) {
                            VMGlobal *g = ctx->vm->globals;
                            while(g) {
                                if (strcmp(g->name, inst->op2->val.str_val) == 0) {
                                    ptr = g->ptr_val;
                                    break;
                                }
                                g = g->next;
                            }
                            if (!ptr) {
                                VMGlobal *vg = arena_alloc(ctx->vm->arena, sizeof(VMGlobal));
                                vg->name = arena_strdup(ctx->vm->arena, inst->op2->val.str_val);
                                vg->ptr_val = arena_alloc(ctx->vm->arena, 1024);
                                vg->next = ctx->vm->globals;
                                ctx->vm->globals = vg;
                                ptr = vg->ptr_val;
                            }
                        }
                        else if (inst->op2->kind == ALIR_VAL_VAR) ptr = (void*)(intptr_t)meta_vm_resolve_var(inst->op2, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                        
                        void *src_ptr = NULL;
                        if (inst->op1->type.base == TYPE_CLASS && inst->op1->type.ptr_depth == 0) {
                            if (inst->op1->kind == ALIR_VAL_TEMP) src_ptr = ctx->registers[inst->op1->temp_id].as.ptr_val;
                            else if (inst->op1->kind == ALIR_VAL_VAR) src_ptr = (void*)(intptr_t)meta_vm_resolve_var(inst->op1, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                        }
                        
                        if (ptr) {
                            if (src_ptr) {
                                memcpy(ptr, src_ptr, 1024);
                            } else {
                                *((long long*)ptr) = val;
                            }
                        }
                    }
                    break;
                }
                    break;
case ALIR_OP_LOAD: {
                    if (inst->dest && inst->op1) { // dest = value, op1 = ptr
                        void *ptr = NULL;
                        if (inst->op1->kind == ALIR_VAL_TEMP) ptr = ctx->registers[inst->op1->temp_id].as.ptr_val;
                        else if (inst->op1->kind == ALIR_VAL_VAR) ptr = (void*)(intptr_t)meta_vm_resolve_var(inst->op1, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                        else if (inst->op1->kind == ALIR_VAL_GLOBAL) {
                            VMGlobal *g = ctx->vm->globals;
                            while(g) {
                                if (strcmp(g->name, inst->op1->val.str_val) == 0) {
                                    ptr = g->ptr_val;
                                    break;
                                }
                                g = g->next;
                            }
// if (!ptr) printf("DEBUG: Global '%s' NOT FOUND in LOAD!\n", inst->op1->val.str_val);
                        }
                        if (ptr) {
                            if (inst->dest->type.base == TYPE_CLASS && inst->dest->type.ptr_depth == 0) {
                                void *copy = arena_alloc(ctx->vm->arena, 1024);
                                memcpy(copy, ptr, 1024);
                                ctx->registers[inst->dest->temp_id].as.ptr_val = copy;
                            } else {
                                ctx->registers[inst->dest->temp_id].as.int_val = *((long long*)ptr);
                            }
                        }
                    }
                    break;
                }
                    break;
case ALIR_OP_GET_PTR: {
                    if (inst->dest && inst->op1 && inst->op2) {
                        void *base_ptr = NULL;
                        if (inst->op1->kind == ALIR_VAL_TEMP) base_ptr = ctx->registers[inst->op1->temp_id].as.ptr_val;
                        else if (inst->op1->kind == ALIR_VAL_VAR) base_ptr = (void*)(intptr_t)meta_vm_resolve_var(inst->op1, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                        else if (inst->op1->kind == ALIR_VAL_GLOBAL) {
                            VMGlobal *g = ctx->vm->globals;
                            while(g) {
                                if (strcmp(g->name, inst->op1->val.str_val) == 0) { base_ptr = g->ptr_val; break; }
                                g = g->next;
                            }
                        }
                        
                        long long offset = 0;
                        if (inst->op2->kind == ALIR_VAL_CONST) offset = inst->op2->val.long_long_val;
                        else if (inst->op2->kind == ALIR_VAL_TEMP) offset = ctx->registers[inst->op2->temp_id].as.int_val;
                        else if (inst->op2->kind == ALIR_VAL_VAR) offset = meta_vm_resolve_var(inst->op2, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                        
// fprintf(stderr, "DEBUG: ALIR_OP_GET_PTR offset=%lld\n", offset);
                        
                        // Treat offset as index into 8-byte array
                        if (base_ptr) {
                            ctx->registers[inst->dest->temp_id].as.ptr_val = (void*)((char*)base_ptr + (offset * 8));
                        }
                    }
                    break;
                }
                    break;
        
case ALIR_OP_FREE_STACK:
    // Memory is tracked via arena in MetaVM, nothing to explicitly free
    break;

default: break;
    }
}
