#include "meta/vm.h"
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
typedef struct {
    union {
        long long int_val;
        double float_val;
        void *ptr_val; // Memory or string pointer
    } as;
} VMValue;

MetaVM* meta_vm_init() {
    MetaVM *vm = calloc(1, sizeof(MetaVM));
    vm->registers = calloc(MAX_VM_STACK, sizeof(VMValue));
    vm->status = 0;
    return vm;
}

void meta_vm_free(MetaVM *vm) {
    if (vm) {
        if (vm->registers) free(vm->registers);
        free(vm);
    }
}

// Very basic linear executor for an ALIR Block
static AlirBlock* find_block(AlirFunction *func, const char *label) {
    AlirBlock *curr = func->blocks;
    while(curr) {
        if (curr->label && strcmp(curr->label, label) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

int meta_vm_execute(MetaVM *vm, AlirModule *module, AlirFunction *func, void *sem_ctx_ptr) {
    if (!func || !vm) return 0;
    SemanticCtx *sem_ctx = (SemanticCtx *)sem_ctx_ptr;
    
    VMValue *registers = (VMValue*) vm->registers;
    vm->status = 0;
    
    AlirBlock *curr_block = func->blocks;
    while (curr_block) {
        AlirBlock *next_block = curr_block->next; // Default fallthrough
        AlirInst *inst = curr_block->head;
        while (inst) {
            switch (inst->op) {
                case ALIR_OP_ALLOCA: {
                    if (inst->dest) {
                        // Allocate small 8 byte slot for local variable in memory heap
                        registers[inst->dest->temp_id].as.ptr_val = calloc(1, 8); 
                    }
                    break;
                }
                case ALIR_OP_STORE: {
                    if (inst->op1 && inst->op2) { // op1 = value, op2 = ptr
                        long long val = 0;
                        if (inst->op1->kind == ALIR_VAL_CONST) val = inst->op1->val.int_val;
                        else if (inst->op1->kind == ALIR_VAL_TEMP) val = registers[inst->op1->temp_id].as.int_val;
                        
                        void *ptr = registers[inst->op2->temp_id].as.ptr_val;
                        if (ptr) *((long long*)ptr) = val;
                    }
                    break;
                }
                case ALIR_OP_LOAD: {
                    if (inst->dest && inst->op1) { // dest = value, op1 = ptr
                        void *ptr = registers[inst->op1->temp_id].as.ptr_val;
                        if (ptr) registers[inst->dest->temp_id].as.int_val = *((long long*)ptr);
                    }
                    break;
                }
                case ALIR_OP_ADD:
                case ALIR_OP_SUB:
                case ALIR_OP_MUL:
                case ALIR_OP_DIV:
                case ALIR_OP_LT:
                case ALIR_OP_GT:
                case ALIR_OP_EQ:
                case ALIR_OP_NEQ: {
                    if (inst->dest && inst->op1 && inst->op2) {
                        long long v1 = (inst->op1->kind == ALIR_VAL_CONST) ? inst->op1->val.int_val : registers[inst->op1->temp_id].as.int_val;
                        long long v2 = (inst->op2->kind == ALIR_VAL_CONST) ? inst->op2->val.int_val : registers[inst->op2->temp_id].as.int_val;
                        long long res = 0;
                        if (inst->op == ALIR_OP_ADD) res = v1 + v2;
                        else if (inst->op == ALIR_OP_SUB) res = v1 - v2;
                        else if (inst->op == ALIR_OP_MUL) res = v1 * v2;
                        else if (inst->op == ALIR_OP_DIV) {
                            if (v2 != 0) res = v1 / v2;
                            else {
                                if (sem_ctx) {
                                    ASTNode fake_node = {0};
                                    fake_node.line = inst->line;
                                    fake_node.col = inst->col;
                                    sem_error(sem_ctx, &fake_node, "Division by zero during compile-time meta execution");
                                }
                                vm->status = 1; // Division by zero
                                goto vm_end;
                            }
                        }
                        else if (inst->op == ALIR_OP_LT) res = v1 < v2;
                        else if (inst->op == ALIR_OP_GT) res = v1 > v2;
                        else if (inst->op == ALIR_OP_EQ) res = v1 == v2;
                        else if (inst->op == ALIR_OP_NEQ) res = v1 != v2;
                        registers[inst->dest->temp_id].as.int_val = res;
                    }
                    break;
                }
                case ALIR_OP_JUMP: {
                    if (inst->op1 && inst->op1->kind == ALIR_VAL_LABEL) {
                        next_block = find_block(func, inst->op1->val.str_val);
                    }
                    break;
                }
                case ALIR_OP_CONDI: {
                    long long cond = 0;
                    if (inst->op1->kind == ALIR_VAL_TEMP) cond = registers[inst->op1->temp_id].as.int_val;
                    else if (inst->op1->kind == ALIR_VAL_CONST) cond = inst->op1->val.int_val;
                    
                    if (cond) {
                        if (inst->op2 && inst->op2->kind == ALIR_VAL_LABEL) 
                            next_block = find_block(func, inst->op2->val.str_val);
                    } else {
                        if (inst->arg_count > 0 && inst->args[0]->kind == ALIR_VAL_LABEL)
                            next_block = find_block(func, inst->args[0]->val.str_val);
                    }
                    break;
                }
                case ALIR_OP_DEFINED: {
                    int is_defined = 0;
                    if (inst->op1 && inst->op1->kind == ALIR_VAL_STRING) {
                        const char *sym_name = inst->op1->val.str_val;
                        
                        // Check globals
                        if (module) {
                            AlirGlobal *g = module->globals;
                            while (g) {
                                if (strcmp(g->name, sym_name) == 0) { is_defined = 1; break; }
                                g = g->next;
                            }
                            if (!is_defined) {
                                AlirFunction *f = module->functions;
                                while (f) {
                                    if (strcmp(f->name, sym_name) == 0) { is_defined = 1; break; }
                                    f = f->next;
                                }
                            }
                        }
                        // Check semantics and macros
                        if (!is_defined && sem_ctx) {
                            SemScope *dummy;
                            if (sem_symbol_lookup((SemanticCtx*)sem_ctx, sym_name, &dummy)) {
                                is_defined = 1;
                            }
                            if (!is_defined) {
                                SemanticCtx *sctx = (SemanticCtx*)sem_ctx;
                                if (sctx->compiler_ctx && sctx->compiler_ctx->macro_head) {
                                    struct MacroDummy {
                                        char *name;
                                        char **params;
                                        int param_count;
                                        void *body;
                                        int body_len;
                                        struct MacroDummy *next;
                                    };
                                    struct MacroDummy *m = (struct MacroDummy *)sctx->compiler_ctx->macro_head;
                                    while (m) {
                                        if (strcmp(m->name, sym_name) == 0) { is_defined = 1; break; }
                                        m = m->next;
                                    }
                                }
                            }
                        }
                    }
                    if (inst->dest) {
                        registers[inst->dest->temp_id].as.int_val = is_defined;
                    }
                    break;
                }
                case ALIR_OP_CALL: {
                    if (inst->op1 && inst->op1->kind == ALIR_VAL_VAR) {
                        if (strcmp(inst->op1->val.str_val, "print") == 0) {
                            for (int i = 0; i < inst->arg_count; i++) {
                                AlirValue *arg = inst->args[i];
                                if (arg->kind == ALIR_VAL_CONST) {
                                    if (arg->type.base == TYPE_INT) printf("%d", arg->val.int_val);
                                    else if (arg->type.base == TYPE_STRING) printf("%s", arg->val.str_val);
                                } else if (arg->kind == ALIR_VAL_TEMP) {
                                    printf("%lld", registers[arg->temp_id].as.int_val);
                                } else if (arg->kind == ALIR_VAL_GLOBAL && module) {
                                    AlirGlobal *g = module->globals;
                                    while (g) {
                                        if (strcmp(g->name, arg->val.str_val) == 0 && g->string_content) {
                                            printf("%s", g->string_content);
                                            break;
                                        }
                                        g = g->next;
                                    }
                                }
                            }
                            printf("\n");
                        }
#ifdef HAVE_LIBFFI
#ifndef _WIN32
                        else {
                            void *func_ptr = dlsym(RTLD_DEFAULT, inst->op1->val.str_val);
                            if (func_ptr) {
                                ffi_cif cif;
                                ffi_type **arg_types = malloc(inst->arg_count * sizeof(ffi_type *));
                                void **arg_values = malloc(inst->arg_count * sizeof(void *));
                                
                                for (int i = 0; i < inst->arg_count; i++) {
                                    AlirValue *arg = inst->args[i];
                                    if (arg->type.base == TYPE_INT || arg->type.base == TYPE_BOOL) {
                                        arg_types[i] = &ffi_type_sint64;
                                        long long *val = malloc(sizeof(long long));
                                        if (arg->kind == ALIR_VAL_CONST) *val = arg->val.int_val;
                                        else if (arg->kind == ALIR_VAL_TEMP) *val = registers[arg->temp_id].as.int_val;
                                        arg_values[i] = val;
                                    } else if (arg->type.base == TYPE_STRING || arg->type.base == TYPE_AUTO) {
                                        arg_types[i] = &ffi_type_pointer;
                                        void **val = malloc(sizeof(void*));
                                        *val = NULL;
                                        if (arg->kind == ALIR_VAL_CONST) *val = arg->val.str_val;
                                        else if (arg->kind == ALIR_VAL_GLOBAL && module) {
                                            AlirGlobal *g = module->globals;
                                            while(g) {
                                                if (strcmp(g->name, arg->val.str_val) == 0 && g->string_content) {
                                                    *val = g->string_content;
                                                    break;
                                                }
                                                g = g->next;
                                            }
                                        }
                                        arg_values[i] = val;
                                    } else {
                                        arg_types[i] = &ffi_type_void;
                                        arg_values[i] = NULL;
                                    }
                                }
                                
                                ffi_type *ret_type = &ffi_type_void;
                                if (inst->dest) {
                                     ret_type = &ffi_type_sint64;
                                }

                                if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, inst->arg_count, ret_type, arg_types) == FFI_OK) {
                                    long long rc = 0;
                                    ffi_call(&cif, func_ptr, &rc, arg_values);
                                    if (inst->dest) {
                                         registers[inst->dest->temp_id].as.int_val = rc;
                                    }
                                }
                                
                                for (int i=0; i<inst->arg_count; i++) if (arg_values[i]) free(arg_values[i]);
                                free(arg_types);
                                free(arg_values);
                            } else {
                                // Extern function not found
                                if (sem_ctx) {
                                    ASTNode fake_node = {0};
                                    fake_node.line = inst->line;
                                    fake_node.col = inst->col;
                                    sem_error(sem_ctx, &fake_node, "Extern C function '%s' not found during compile-time execution", inst->op1->val.str_val);
                                }
                                vm->status = 1;
                                goto vm_end;
                            }
                        }
#endif
#endif
                    }
                    break;
                }
                default:
                    break;
            }
            // Break inner instruction loop if we jumped
            if (inst->op == ALIR_OP_JUMP || inst->op == ALIR_OP_CONDI) break;
            
            inst = inst->next;
        }
        curr_block = next_block;
    }
    
vm_end:
    return vm->status;
}
