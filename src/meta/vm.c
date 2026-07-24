#include "meta/vm.h"
#include "meta/vm_internal.h"
#include "alir/alir.h"
#include "common/diagnostic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_LIBFFI
#include <ffi.h>
#endif
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include "semantic/semantic.h"

#define MAX_VM_STACK 1024

// Represents a value in the ALIR Virtual Machine memory
MetaVM* meta_vm_init(Arena *arena) {
    MetaVM *vm = arena_alloc(arena, sizeof(MetaVM));
    vm->arena = arena;
    vm->registers = arena_alloc(arena, MAX_VM_STACK * sizeof(VMValue));
    vm->globals = NULL;
    vm->status = 0;
    return vm;
}

void meta_vm_free(MetaVM *vm) {
    (void)vm;
    // Handled by Arena
}

// Very basic linear executor for an ALIR Block
AlirBlock* find_block(AlirFunction *func, const char *label) {
    AlirBlock *curr = func->blocks;
    while(curr) {
        if (curr->label && strcmp(curr->label, label) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}
long long meta_vm_resolve_var(AlirValue *val, AlirModule *module, MetaVM *vm, long long *args, int arg_count) {
    if (!val || val->kind != ALIR_VAL_VAR) return 0;
    const char *name = val->val.str_val;
    if (name[0] == 'p') {
        int idx = atoi(name + 1);
        if (idx < arg_count && args) return args[idx];
    }
    if (module) {
        AlirGlobal *g = module->globals;
        while(g) {
            if (strcmp(g->name, name) == 0) return (long long)(intptr_t)g->string_content;
            g = g->next;
        }
    }
    VMGlobal *g = vm->globals;
    while(g) {
        if (strcmp(g->name, name) == 0) return (long long)(intptr_t)g->ptr_val;
        g = g->next;
    }
    return 0;
}
long long meta_vm_execute(MetaVM *vm, AlirModule *module, AlirFunction *func, void *sem_ctx_ptr, long long *args, int arg_count) {
    if (!vm || !func) return 0;
    
    if (strcmp(func->name, "Vector_as_int") == 0) {
        AlirInst *i = func->blocks ? func->blocks->head : NULL;
        while(i) {
            i = i->next;
        }
    }
    SemanticCtx *sem_ctx = (SemanticCtx *)sem_ctx_ptr;
    
    VMValue local_registers[MAX_VM_STACK];
    memset(local_registers, 0, sizeof(local_registers));
    VMValue *old_registers = vm->registers;
    vm->registers = local_registers;
    VMValue *registers = local_registers;
    
    long long ret_val = 0;
    vm->status = 0;
    
    AlirBlock *curr_block = func->blocks;
    while (curr_block) {
        AlirBlock *next_block = curr_block->next; // Default fallthrough
        AlirInst *inst = curr_block->head;
        while (inst) {

            VMContext ctx = {
                .vm = vm,
                .module = module,
                .func = func,
                .sem_ctx = sem_ctx,
                .args = args,
                .arg_count = arg_count,
                .registers = registers,
                .next_block = &next_block,
                .ret_val = &ret_val
            };

            switch (inst->op) {
                case ALIR_OP_ALLOCA:
                case ALIR_OP_STORE:
                case ALIR_OP_LOAD:
                case ALIR_OP_GET_PTR:
                case ALIR_OP_FREE_STACK:
                    vm_eval_mem(&ctx, inst);
                    break;
                case ALIR_OP_ADD:
                case ALIR_OP_SUB:
                case ALIR_OP_MUL:
                case ALIR_OP_DIV:
                case ALIR_OP_MOD:
                case ALIR_OP_ROTL:
                case ALIR_OP_ROTR:
                case ALIR_OP_SHL:
                case ALIR_OP_SHR:
                case ALIR_OP_OR:
                case ALIR_OP_AND:
                case ALIR_OP_XOR:
                case ALIR_OP_NOT:
                case ALIR_OP_EQ:
                case ALIR_OP_NEQ:
                case ALIR_OP_LT:
                case ALIR_OP_LTE:
                case ALIR_OP_GT:
                case ALIR_OP_GTE:
                case ALIR_OP_FADD:
                case ALIR_OP_FSUB:
                case ALIR_OP_FMUL:
                case ALIR_OP_FDIV:
                    vm_eval_math(&ctx, inst);
                    break;
                case ALIR_OP_JUMP:
                case ALIR_OP_SWITCH:
                case ALIR_OP_CONDI:
                case ALIR_OP_RET:
                case ALIR_OP_YIELD:
                    vm_eval_flow(&ctx, inst);
                    break;
                case ALIR_OP_CALL:
                    vm_eval_call(&ctx, inst);
                    break;
                case ALIR_OP_DEFINED:
                case ALIR_OP_SIZEOF:
                case ALIR_OP_ALIGNOF:
                case ALIR_OP_TYPEOF:
                case ALIR_OP_CAST:
                case ALIR_OP_BITCAST:
                case ALIR_OP_MOV:
                case ALIR_OP_PHI:
                case ALIR_OP_FALLBACK:
                case ALIR_OP_ITER_INIT:
                case ALIR_OP_ITER_VALID:
                case ALIR_OP_ITER_NEXT:
                case ALIR_OP_ITER_GET:
                    vm_eval_misc(&ctx, inst);
                    break;
                default: break;
            }
            if (inst->op == ALIR_OP_PANIC) {
                // Compile-time panic
                if (inst->op1) {
                    if (inst->op1->kind == ALIR_VAL_GLOBAL && inst->op1->val.str_val && module) {
                        AlirGlobal *g = module->globals;
                        while(g) {
                            if (strcmp(g->name, inst->op1->val.str_val) == 0) {
                                fprintf(stderr, "Compile-time purge: %s\n", g->string_content);
                                break;
                            }
                            g = g->next;
                        }
                    } else if (inst->op1->kind == ALIR_VAL_TEMP) {
                        fprintf(stderr, "Compile-time purge: %lld\n", registers[inst->op1->temp_id].as.int_val);
                    } else {
                        fprintf(stderr, "Compile-time purge executed.\n");
                    }
                } else {
                    fprintf(stderr, "Compile-time purge executed.\n");
                }
                exit(1);
            }
            
            if (ctx.should_return) {
                vm->registers = old_registers;
                return ret_val;
            }
            
            inst = inst->next;
        }
        curr_block = next_block;
    }
    ret_val = vm->status;

    
    vm->registers = old_registers;
    return ret_val;
}
