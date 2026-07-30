#include "vm.h"
#include "vm_internal.h"
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

MetalirVM* metalir_vm_init(Arena *arena) {
    MetalirVM *vm = arena_alloc(arena, sizeof(MetalirVM));
    vm->arena = arena;
    vm->registers = arena_alloc(arena, MAX_VM_STACK * sizeof(VMValue));
    vm->globals = NULL;
    vm->status = 0;
    return vm;
}

void metalir_vm_free(MetalirVM *vm) {
    (void)vm;
}

AlirBlock* find_block(AlirFunction *func, const char *label) {
    AlirBlock *curr = func->blocks;
    while(curr) {
        if (curr->label && streq(curr->label, label)) return curr;
        curr = curr->next;
    }
    return NULL;
}

long long metalir_vm_resolve_var(AlirValue *val, AlirModule *module, MetalirVM *vm, long long *args, int arg_count) {
    if (!val) return 0;
    if (val->kind == ALIR_VAL_VAR) {
        const char *name = val->val.str_val;
        if (name && name[0] == 'p') {
            int idx = atoi(name + 1);
            if (idx < arg_count && args) return args[idx];
        }
        if (vm) {
            VMGlobal *g = vm->globals;
            while(g) {
                if (streq(g->name, name)) return (long long)(intptr_t)g->ptr_val;
                g = g->next;
            }
        }
        if (module) {
            AlirGlobal *g = module->globals;
            while(g) {
                if (streq(g->name, name)) return (long long)(intptr_t)g->string_content;
                g = g->next;
            }
        }
    } else if (val->kind == ALIR_VAL_GLOBAL) {
        const char *name = val->val.str_val;
        if (vm) {
            VMGlobal *g = vm->globals;
            while(g) {
                if (streq(g->name, name)) return (long long)(intptr_t)g->ptr_val;
                g = g->next;
            }
        }
        if (module) {
            AlirGlobal *g = module->globals;
            while(g) {
                if (streq(g->name, name)) return (long long)(intptr_t)g->string_content;
                g = g->next;
            }
        }
    }
    return 0;
}

long long metalir_vm_execute(MetalirVM *vm, AlirModule *module, AlirFunction *func, void *sem_ctx_ptr, long long *args, int arg_count) {
    if (!vm || !func) return 0;

    // What the fuck is this
    /*
    if (streq(func->name, "Vector_as_int")) {
        AlirInst *i = func->blocks ? func->blocks->head : NULL;
        while(i) {
            i = i->next;
        }
    }*/
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
        AlirBlock *next_block = curr_block->next;
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
                case ALIR_OP_CONDI:
                case ALIR_OP_RET:
                    vm_eval_flow(&ctx, inst);
                    break;
                case ALIR_OP_CALL:
                    vm_eval_call(&ctx, inst);
                    break;
                case ALIR_OP_CAST:
                case ALIR_OP_BITCAST:
                case ALIR_OP_FALLBACK:
                case ALIR_OP_SIZEOF:
                case ALIR_OP_ALIGNOF:
                    vm_eval_misc(&ctx, inst);
                    break;
                default: break;
            }
            if (inst->op == ALIR_OP_PANIC) {
                if (inst->op1) {
                    if (inst->op1->kind == ALIR_VAL_GLOBAL && inst->op1->val.str_val && module) {
                        AlirGlobal *g = module->globals;
                        while(g) {
                            if (streq(g->name, inst->op1->val.str_val)) {
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
