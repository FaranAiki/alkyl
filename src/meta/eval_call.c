#include "meta/vm_internal.h"
#include "common/arena.h"
#include <string.h>
#include <stdio.h>
#ifdef HAVE_LIBFFI
#include <ffi.h>
#endif
#ifndef _WIN32
#include <dlfcn.h>
#endif

void vm_eval_call(VMContext *ctx, AlirInst *inst) {
    switch(inst->op) {
case ALIR_OP_CALL: {
                    if (inst->op1 && (inst->op1->kind == ALIR_VAL_VAR || inst->op1->kind == ALIR_VAL_GLOBAL)) {
                        if (inst->op1->val.str_val && strcmp(inst->op1->val.str_val, "print") == 0) {
                            for (int i = 0; i < inst->arg_count; i++) {
                                AlirValue *arg = inst->args[i];
                                if (arg->kind == ALIR_VAL_CONST) {
                                    if (arg->type.base == TYPE_INT) printf("%lld", arg->val.long_long_val);
                                    else if (arg->type.base == TYPE_CLASS && arg->type.class_name && strcmp(arg->type.class_name, "string") == 0) printf("%s", arg->val.str_val);
                                } else if (arg->kind == ALIR_VAL_TEMP) {
                                    printf("%lld", ctx->registers[arg->temp_id].as.int_val);
                                } else if (arg->kind == ALIR_VAL_VAR) {
                                    printf("%lld", meta_vm_resolve_var(arg, ctx->module, ctx->vm, ctx->args, ctx->arg_count));
                                } else if (arg->kind == ALIR_VAL_GLOBAL && ctx->module) {
                                    AlirGlobal *g = ctx->module->globals;
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
                            if (ctx->module) {
                                AlirFunction *f = ctx->module->functions;
                                while(f) {
                                    if (strcmp(f->name, inst->op1->val.str_val) == 0) {
                                        target_fn = f;
                                        break;
                                    }
                                    f = f->next;
                                }
                            }
                            
                            if (target_fn) {
                                int __na_sz = inst->arg_count > 0 ? inst->arg_count : 1; long long new_args[__na_sz];
                                for (int i = 0; i < inst->arg_count; i++) {
                                    AlirValue *arg = inst->args[i];
                                    if (arg->kind == ALIR_VAL_CONST) new_args[i] = arg->val.long_long_val;
                                    else if (arg->kind == ALIR_VAL_TEMP) new_args[i] = ctx->registers[arg->temp_id].as.int_val;
                                    else if (arg->kind == ALIR_VAL_VAR) new_args[i] = meta_vm_resolve_var(arg, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                                    else new_args[i] = 0;
                                }
// fprintf(stderr, "DEBUG: Calling internal ctx->func %s with arg %lld\n", target_fn->name, inst->arg_count > 0 ? new_args[0] : -1);
                                long long rc = meta_vm_execute(ctx->vm, ctx->module, target_fn, ctx->sem_ctx, new_args, inst->arg_count);
                                if (strcmp(target_fn->name, "Vector_as_int") == 0) {
                                }
                                
                                if (inst->dest) {
                                    ctx->registers[inst->dest->temp_id].as.int_val = rc;
// fprintf(stderr, "DEBUG: Stored %lld to %d\n", rc, inst->dest->temp_id);
                                }
                            } else {
                                void *func_ptr = dlsym(RTLD_DEFAULT, inst->op1->val.str_val);
                                if (func_ptr) {
                                    ffi_cif cif;
                                    int __at_sz = inst->arg_count > 0 ? inst->arg_count : 1; ffi_type *arg_types[__at_sz];
                                    int __av_sz = inst->arg_count > 0 ? inst->arg_count : 1; void *arg_values[__av_sz];
                                    uint64_t arg_data[__av_sz];
                                    
                                    for (int i = 0; i < inst->arg_count; i++) {
                                        AlirValue *arg = inst->args[i];
                                        if (arg->type.base == TYPE_INT || arg->type.base == TYPE_BOOL) {
                                            arg_types[i] = &ffi_type_sint64;
                                            long long *val = (long long*)&arg_data[i];
                                            if (arg->kind == ALIR_VAL_CONST) *val = arg->val.long_long_val;
                                            else if (arg->kind == ALIR_VAL_TEMP) *val = ctx->registers[arg->temp_id].as.int_val;
                                            else if (arg->kind == ALIR_VAL_VAR) *val = meta_vm_resolve_var(arg, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                                            arg_values[i] = val;
                                        } else if (arg->type.base == TYPE_DOUBLE || arg->type.base == TYPE_SINGLE) {
                                            arg_types[i] = (arg->type.base == TYPE_DOUBLE) ? &ffi_type_double : &ffi_type_float;
                                            void *val = &arg_data[i];
                                            if (arg->kind == ALIR_VAL_CONST) {
                                                if (arg->type.base == TYPE_DOUBLE) *(double*)val = arg->val.double_val;
                                                else *(float*)val = arg->val.single_val;
                                            } else if (arg->kind == ALIR_VAL_TEMP) {
                                                if (arg->type.base == TYPE_DOUBLE) *(double*)val = ctx->registers[arg->temp_id].as.single_val;
                                                else *(float*)val = (float)ctx->registers[arg->temp_id].as.single_val;
                                            } else if (arg->kind == ALIR_VAL_VAR) {
                                                long long raw = meta_vm_resolve_var(arg, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                                                if (arg->type.base == TYPE_DOUBLE) memcpy(val, &raw, sizeof(double));
                                                else { float f; memcpy(&f, &raw, sizeof(float)); *(float*)val = f; }
                                            }
                                            arg_values[i] = val;
                                        } else if ((arg->type.base == TYPE_CLASS && arg->type.class_name && strcmp(arg->type.class_name, "string") == 0) || arg->type.base == TYPE_AUTO || arg->type.ptr_depth > 0) {
                                            arg_types[i] = &ffi_type_pointer;
                                            void **val = (void**)&arg_data[i];
                                            *val = NULL;
                                            if (arg->kind == ALIR_VAL_CONST) *val = (void*)arg->val.str_val;
                                            else if (arg->kind == ALIR_VAL_VAR) *val = (void*)(intptr_t)meta_vm_resolve_var(arg, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                                            else if (arg->kind == ALIR_VAL_GLOBAL && ctx->module) {
                                                AlirGlobal *g = ctx->module->globals;
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
                                            if (inst->dest->type.base == TYPE_DOUBLE) ctx->registers[inst->dest->temp_id].as.single_val = rc_double;
                                            else if (inst->dest->type.base == TYPE_SINGLE) ctx->registers[inst->dest->temp_id].as.single_val = (double)rc_float;
                                            else ctx->registers[inst->dest->temp_id].as.int_val = rc_int;
                                        }
                                    }
                                } else {
                                    // Extern function not found
                                    if (ctx->sem_ctx) {
                                        ASTNode fake_node = {0};
                                        fake_node.line = inst->line;
                                        fake_node.col = inst->col;
                                        sem_error(ctx->sem_ctx, &fake_node, "Extern C function '%s' not found during compile-time execution", inst->op1->val.str_val);
                                    }
                                    ctx->vm->status = 1;
                                    (*ctx->ret_val) = ctx->vm->status; ctx->should_return = 1; return;
                                }
                            }
                        }
#endif
#endif
                    }
                    break;
                }
                    break;
        default: break;
    }
}
