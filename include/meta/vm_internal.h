#ifndef VM_INTERNAL_H
#define VM_INTERNAL_H

#include "meta/vm.h"
#include "alir/alir.h"
#include "semantic/semantic.h"

#define MAX_VM_STACK 1024

typedef struct {
    union {
        long long int_val;
        double single_val;
        void *ptr_val;
    } as;
} VMValue;

typedef struct {
    MetaVM *vm;
    AlirModule *module;
    AlirFunction *func;
    SemanticCtx *sem_ctx;
    long long *args;
    int arg_count;
    
    VMValue *registers;
    AlirBlock **next_block;
    long long *ret_val;
    int should_return;
} VMContext;

long long meta_vm_resolve_var(AlirValue *val, AlirModule *module, MetaVM *vm, long long *args, int arg_count);

void vm_eval_mem(VMContext *ctx, AlirInst *inst);
void vm_eval_math(VMContext *ctx, AlirInst *inst);
void vm_eval_call(VMContext *ctx, AlirInst *inst);
void vm_eval_flow(VMContext *ctx, AlirInst *inst);
void vm_eval_misc(VMContext *ctx, AlirInst *inst);

AlirBlock* find_block(AlirFunction *func, const char *label);

#endif
