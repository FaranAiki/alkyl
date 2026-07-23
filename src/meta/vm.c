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
        double single_val;
        void *ptr_val; // Memory or string pointer
    } as;
} VMValue;

MetaVM* meta_vm_init(Arena *arena) {
    MetaVM *vm = arena_alloc(arena, sizeof(MetaVM));
    vm->arena = arena;
    vm->registers = arena_alloc(arena, MAX_VM_STACK * sizeof(VMValue));
    vm->status = 0;
    return vm;
}

void meta_vm_free(MetaVM *vm) {
    (void)vm;
    // Handled by Arena
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
    if (!func || !vm) return 0;
    SemanticCtx *sem_ctx = (SemanticCtx *)sem_ctx_ptr;
    
    VMValue *old_registers = vm->registers;
    vm->registers = calloc(MAX_VM_STACK, sizeof(VMValue));
    VMValue *registers = (VMValue*) vm->registers;
    long long ret_val = 0;
    vm->status = 0;
    
    AlirBlock *curr_block = func->blocks;
    while (curr_block) {
        AlirBlock *next_block = curr_block->next; // Default fallthrough
        AlirInst *inst = curr_block->head;
        while (inst) {
            switch (inst->op) {
                case ALIR_OP_ALLOCA: {
                    if (inst->dest) {
                        registers[inst->dest->temp_id].as.ptr_val = arena_alloc(vm->arena, 1024); 
                    }
                    break;
                }
                case ALIR_OP_STORE: {
                    if (inst->op1 && inst->op2) { // op1 = value, op2 = ptr
                        long long val = 0;
                        if (inst->op1->kind == ALIR_VAL_CONST) val = inst->op1->val.long_long_val;
                        else if (inst->op1->kind == ALIR_VAL_TEMP) val = registers[inst->op1->temp_id].as.int_val;
                        else if (inst->op1->kind == ALIR_VAL_VAR) val = meta_vm_resolve_var(inst->op1, module, vm, args, arg_count);
                        else if (inst->op1->kind == ALIR_VAL_GLOBAL && module) {
                            AlirGlobal *g = module->globals;
                            while(g) {
                                if (strcmp(g->name, inst->op1->val.str_val) == 0) {
                                    val = (long long)(intptr_t)g->string_content;
                                    break;
                                }
                                g = g->next;
                            }
                        }
                        
                        void *ptr = NULL;
                        if (inst->op2->kind == ALIR_VAL_TEMP) ptr = registers[inst->op2->temp_id].as.ptr_val;
                        else if (inst->op2->kind == ALIR_VAL_GLOBAL) {
                            VMGlobal *g = vm->globals;
                            while(g) {
                                if (strcmp(g->name, inst->op2->val.str_val) == 0) {
                                    ptr = g->ptr_val;
                                    break;
                                }
                                g = g->next;
                            }
                            if (!ptr) {
                                VMGlobal *vg = calloc(1, sizeof(VMGlobal));
                                vg->name = strdup(inst->op2->val.str_val);
                                vg->ptr_val = calloc(1, 1024);
                                vg->next = vm->globals;
                                vm->globals = vg;
                                ptr = vg->ptr_val;
                            }
                        }
                        else if (inst->op2->kind == ALIR_VAL_VAR) ptr = (void*)(intptr_t)meta_vm_resolve_var(inst->op2, module, vm, args, arg_count);
                        
                        if (ptr) *((long long*)ptr) = val;
                    }
                    break;
                }
                case ALIR_OP_GET_PTR: {
                    if (inst->dest && inst->op1 && inst->op2) {
                        void *base_ptr = NULL;
                        if (inst->op1->kind == ALIR_VAL_TEMP) base_ptr = registers[inst->op1->temp_id].as.ptr_val;
                        else if (inst->op1->kind == ALIR_VAL_VAR) base_ptr = (void*)(intptr_t)meta_vm_resolve_var(inst->op1, module, vm, args, arg_count);
                        else if (inst->op1->kind == ALIR_VAL_GLOBAL) {
                            VMGlobal *g = vm->globals;
                            while(g) {
                                if (strcmp(g->name, inst->op1->val.str_val) == 0) { base_ptr = g->ptr_val; break; }
                                g = g->next;
                            }
                        }
                        
                        long long offset = 0;
                        if (inst->op2->kind == ALIR_VAL_CONST) offset = inst->op2->val.long_long_val;
                        else if (inst->op2->kind == ALIR_VAL_TEMP) offset = registers[inst->op2->temp_id].as.int_val;
                        else if (inst->op2->kind == ALIR_VAL_VAR) offset = meta_vm_resolve_var(inst->op2, module, vm, args, arg_count);
                        
// fprintf(stderr, "DEBUG: ALIR_OP_GET_PTR offset=%lld\n", offset);
                        
                        // Treat offset as index into 8-byte array
                        if (base_ptr) {
                            registers[inst->dest->temp_id].as.ptr_val = (void*)((char*)base_ptr + (offset * 8));
                        }
                    }
                    break;
                }
                case ALIR_OP_LOAD: {
                    if (inst->dest && inst->op1) { // dest = value, op1 = ptr
                        void *ptr = NULL;
                        if (inst->op1->kind == ALIR_VAL_TEMP) ptr = registers[inst->op1->temp_id].as.ptr_val;
                        else if (inst->op1->kind == ALIR_VAL_VAR) ptr = (void*)(intptr_t)meta_vm_resolve_var(inst->op1, module, vm, args, arg_count);
                        else if (inst->op1->kind == ALIR_VAL_GLOBAL) {
                            VMGlobal *g = vm->globals;
                            while(g) {
                                if (strcmp(g->name, inst->op1->val.str_val) == 0) {
                                    ptr = g->ptr_val;
                                    break;
                                }
                                g = g->next;
                            }
// if (!ptr) printf("DEBUG: Global '%s' NOT FOUND in LOAD!\n", inst->op1->val.str_val);
                        }
                        if (ptr) registers[inst->dest->temp_id].as.int_val = *((long long*)ptr);
                    }
                    break;
                }
                case ALIR_OP_CAST: {
                    if (inst->dest && inst->op1) {
                        double fval = 0;
                        long long ival = 0;
                        if (inst->op1->kind == ALIR_VAL_CONST) {
                            if (inst->op1->type.base == TYPE_SINGLE) {
                                fval = inst->op1->val.single_val;
                                ival = (long long)fval;
                            } else if (inst->op1->type.base == TYPE_DOUBLE) {
                                fval = inst->op1->val.double_val;
                                ival = (long long)fval;
                            } else {
                                ival = inst->op1->val.long_long_val;
                                fval = (double)ival;
                            }
                        } else if (inst->op1->kind == ALIR_VAL_TEMP) {
                            ival = registers[inst->op1->temp_id].as.int_val;
                            fval = (double)ival;
                        } else if (inst->op1->kind == ALIR_VAL_VAR) {
                            ival = meta_vm_resolve_var(inst->op1, module, vm, args, arg_count);
                            fval = (double)ival;
                        } else {
                            if (inst->op1->type.base == TYPE_SINGLE || inst->op1->type.base == TYPE_DOUBLE) {
                                fval = registers[inst->op1->temp_id].as.single_val;
                                ival = (long long)fval;
                            } else {
                                ival = registers[inst->op1->temp_id].as.int_val;
                                fval = (double)ival;
                            }
                        }
                        if (inst->dest->type.base == TYPE_SINGLE || inst->dest->type.base == TYPE_DOUBLE) {
                            registers[inst->dest->temp_id].as.single_val = fval;
                        } else {
                            registers[inst->dest->temp_id].as.int_val = ival;
                        }
                    }
                    break;
                }
                case ALIR_OP_FADD:
                case ALIR_OP_FSUB:
                case ALIR_OP_FMUL:
                case ALIR_OP_FDIV: {
                    if (inst->dest && inst->op1 && inst->op2) {
                        double v1 = 0, v2 = 0;
                        if (inst->op1->kind == ALIR_VAL_CONST) {
                            if (inst->op1->type.base == TYPE_SINGLE) { v1 = inst->op1->val.single_val; }
                            else { v1 = inst->op1->val.double_val; }
                        } else v1 = registers[inst->op1->temp_id].as.single_val;
                        if (inst->op2->kind == ALIR_VAL_CONST) {
                            if (inst->op2->type.base == TYPE_SINGLE) { v2 = inst->op2->val.single_val; }
                            else { v2 = inst->op2->val.double_val; }
                        } else v2 = registers[inst->op2->temp_id].as.single_val;
                        
                        double res = 0;
                        if (inst->op == ALIR_OP_FADD) res = v1 + v2;
                        else if (inst->op == ALIR_OP_FSUB) res = v1 - v2;
                        else if (inst->op == ALIR_OP_FMUL) res = v1 * v2;
                        else if (inst->op == ALIR_OP_FDIV) res = (v2 != 0) ? (v1 / v2) : 0;
                        registers[inst->dest->temp_id].as.single_val = res;
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
                        long long v1 = (inst->op1->kind == ALIR_VAL_CONST) ? inst->op1->val.long_long_val : registers[inst->op1->temp_id].as.int_val;
                        long long v2 = (inst->op2->kind == ALIR_VAL_CONST) ? inst->op2->val.long_long_val : registers[inst->op2->temp_id].as.int_val;
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
                                return vm->status;
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
                    else if (inst->op1->kind == ALIR_VAL_CONST) cond = inst->op1->val.long_long_val;
                    
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
                    if (inst->op1 && inst->op1->kind == ALIR_VAL_VAR) {
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
                    if (inst->op1 && (inst->op1->kind == ALIR_VAL_VAR || inst->op1->kind == ALIR_VAL_GLOBAL)) {
                        if (strcmp(inst->op1->val.str_val, "print") == 0) {
                            for (int i = 0; i < inst->arg_count; i++) {
                                AlirValue *arg = inst->args[i];
                                if (arg->kind == ALIR_VAL_CONST) {
                                    if (arg->type.base == TYPE_INT) printf("%lld", arg->val.long_long_val);
                                    else if (arg->type.base == TYPE_CLASS && arg->type.class_name && strcmp(arg->type.class_name, "string") == 0) printf("%s", arg->val.str_val);
                                } else if (arg->kind == ALIR_VAL_TEMP) {
                                    printf("%lld", registers[arg->temp_id].as.int_val);
                                } else if (arg->kind == ALIR_VAL_VAR) {
                                    printf("%lld", meta_vm_resolve_var(arg, module, vm, args, arg_count));
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
                            AlirFunction *target_fn = NULL;
                            if (module) {
                                AlirFunction *f = module->functions;
                                while(f) {
                                    if (strcmp(f->name, inst->op1->val.str_val) == 0) {
                                        target_fn = f;
                                        break;
                                    }
                                    f = f->next;
                                }
                            }
                            
                            if (target_fn) {
                                long long *new_args = malloc(sizeof(long long) * inst->arg_count);
                                for (int i = 0; i < inst->arg_count; i++) {
                                    AlirValue *arg = inst->args[i];
                                    if (arg->kind == ALIR_VAL_CONST) new_args[i] = arg->val.long_long_val;
                                    else if (arg->kind == ALIR_VAL_TEMP) new_args[i] = registers[arg->temp_id].as.int_val;
                                    else if (arg->kind == ALIR_VAL_VAR) new_args[i] = meta_vm_resolve_var(arg, module, vm, args, arg_count);
                                    else new_args[i] = 0;
                                }
// fprintf(stderr, "DEBUG: Calling internal func %s with arg %lld\n", target_fn->name, inst->arg_count > 0 ? new_args[0] : -1);
                                long long rc = meta_vm_execute(vm, module, target_fn, sem_ctx_ptr, new_args, inst->arg_count);
// fprintf(stderr, "DEBUG: Returned %lld\n", rc);
                                free(new_args);
                                if (inst->dest) {
                                    registers[inst->dest->temp_id].as.int_val = rc;
// fprintf(stderr, "DEBUG: Stored %lld to %d\n", rc, inst->dest->temp_id);
                                }
                            } else {
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
                                            if (arg->kind == ALIR_VAL_CONST) *val = arg->val.long_long_val;
                                            else if (arg->kind == ALIR_VAL_TEMP) *val = registers[arg->temp_id].as.int_val;
                                            else if (arg->kind == ALIR_VAL_VAR) *val = meta_vm_resolve_var(arg, module, vm, args, arg_count);
                                            arg_values[i] = val;
                                        } else if (arg->type.base == TYPE_DOUBLE || arg->type.base == TYPE_SINGLE) {
                                            arg_types[i] = (arg->type.base == TYPE_DOUBLE) ? &ffi_type_double : &ffi_type_float;
                                            void *val = malloc((arg->type.base == TYPE_DOUBLE) ? sizeof(double) : sizeof(float));
                                            if (arg->kind == ALIR_VAL_CONST) {
                                                if (arg->type.base == TYPE_DOUBLE) *(double*)val = arg->val.double_val;
                                                else *(float*)val = arg->val.single_val;
                                            } else if (arg->kind == ALIR_VAL_TEMP) {
                                                if (arg->type.base == TYPE_DOUBLE) *(double*)val = registers[arg->temp_id].as.single_val;
                                                else *(float*)val = (float)registers[arg->temp_id].as.single_val;
                                            } else if (arg->kind == ALIR_VAL_VAR) {
                                                long long raw = meta_vm_resolve_var(arg, module, vm, args, arg_count);
                                                if (arg->type.base == TYPE_DOUBLE) memcpy(val, &raw, sizeof(double));
                                                else { float f; memcpy(&f, &raw, sizeof(float)); *(float*)val = f; }
                                            }
                                            arg_values[i] = val;
                                        } else if ((arg->type.base == TYPE_CLASS && arg->type.class_name && strcmp(arg->type.class_name, "string") == 0) || arg->type.base == TYPE_AUTO || arg->type.ptr_depth > 0) {
                                            arg_types[i] = &ffi_type_pointer;
                                            void **val = malloc(sizeof(void*));
                                            *val = NULL;
// fprintf(stderr, "DEBUG: string arg kind=%d\n", arg->kind);
                                            if (arg->kind == ALIR_VAL_CONST) *val = (void*)arg->val.str_val;
                                            else if (arg->kind == ALIR_VAL_VAR) *val = (void*)(intptr_t)meta_vm_resolve_var(arg, module, vm, args, arg_count);
                                            else if (arg->kind == ALIR_VAL_GLOBAL && module) {
                                                AlirGlobal *g = module->globals;
                                                while(g) {
                                                    if (strcmp(g->name, arg->val.str_val) == 0 && g->string_content) {
                                                        *val = g->string_content;
                                                        break;
                                                    }
                                                    g = g->next;
                                                }
// fprintf(stderr, "DEBUG: ALIR_VAL_GLOBAL resolved to %p (g->name: %s, arg->str: %s)\n", *val, module->globals ? module->globals->name : "null", arg->val.str_val);
                                            }
                                            arg_values[i] = val;
                                        } else {
                                            arg_types[i] = &ffi_type_void;
                                            arg_values[i] = NULL;
                                        }
                                    }
                                    
                                    ffi_type *ret_type = &ffi_type_void;
                                    if (inst->dest) {
                                        if (inst->dest->type.base == TYPE_DOUBLE) ret_type = &ffi_type_double;
                                        else if (inst->dest->type.base == TYPE_SINGLE) ret_type = &ffi_type_float;
                                        else ret_type = &ffi_type_sint64;
                                    }
    
                                    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, inst->arg_count, ret_type, arg_types) == FFI_OK) {
                                        long long rc_int = 0;
                                        double rc_double = 0;
                                        float rc_float = 0;
                                        void *rc = &rc_int;
                                        if (inst->dest) {
                                            if (inst->dest->type.base == TYPE_DOUBLE) rc = &rc_double;
                                            else if (inst->dest->type.base == TYPE_SINGLE) rc = &rc_float;
                                        }
                                        ffi_call(&cif, func_ptr, rc, arg_values);
                                        if (inst->dest) {
                                            if (inst->dest->type.base == TYPE_DOUBLE) registers[inst->dest->temp_id].as.single_val = rc_double;
                                            else if (inst->dest->type.base == TYPE_SINGLE) registers[inst->dest->temp_id].as.single_val = (double)rc_float;
                                            else registers[inst->dest->temp_id].as.int_val = rc_int;
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
                                    ret_val = vm->status; goto cleanup;
                                }
                            }
                        }
#endif
#endif
                    }
                    break;
                }
                case ALIR_OP_RET: {
                    if (inst->op1) {
                        if (inst->op1->kind == ALIR_VAL_TEMP) { 
                            if (inst->op1->type.base == TYPE_SINGLE) {
                                float f = (float)registers[inst->op1->temp_id].as.single_val;
                                memcpy(&ret_val, &f, sizeof(float));
                            } else if (inst->op1->type.base == TYPE_DOUBLE) {
                                double d = registers[inst->op1->temp_id].as.single_val;
                                memcpy(&ret_val, &d, sizeof(double));
                            } else {
                                ret_val = registers[inst->op1->temp_id].as.int_val; 
                            }
                            goto cleanup; 
                        }
                        else if (inst->op1->kind == ALIR_VAL_CONST) { 
                            if (inst->op1->type.base == TYPE_SINGLE) {
                                float f = inst->op1->val.single_val;
                                memcpy(&ret_val, &f, sizeof(float));
                            } else if (inst->op1->type.base == TYPE_DOUBLE) {
                                double d = inst->op1->val.double_val;
                                memcpy(&ret_val, &d, sizeof(double));
                            } else {
                                ret_val = inst->op1->val.long_long_val;
                            }
                            goto cleanup; 
                        }
                        else if (inst->op1->kind == ALIR_VAL_GLOBAL) {
                            if (module) {
                                AlirGlobal *g = module->globals;
                                while(g) {
                                    if (strcmp(g->name, inst->op1->val.str_val) == 0 && g->string_content) {
                                        ret_val = (intptr_t)g->string_content;
                                        goto cleanup;
                                    }
                                    g = g->next;
                                }
                            }
                        }
                    }
                    ret_val = 0; goto cleanup;
                }
                default:
                    break;
            }
            // Break inner instruction loop if we jumped
            if (inst->op == ALIR_OP_JUMP || inst->op == ALIR_OP_CONDI || inst->op == ALIR_OP_RET) break;
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
            
            inst = inst->next;
        }
        curr_block = next_block;
    }
    ret_val = vm->status;

cleanup:
    free(vm->registers);
    vm->registers = old_registers;
    return ret_val;
}
