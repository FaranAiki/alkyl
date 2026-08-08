#include "vm_internal.h"
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
                        if (inst->op1->val.str_val) {
                            debug_metalir("VM CALL: %s\n", inst->op1->val.str_val);
                        }
                        if (inst->op1->val.str_val && streq_lit(inst->op1->val.str_val, "print")) {
                            for (int i = 0; i < inst->arg_count; i++) {
                                AlirValue *arg = inst->args[i];
                                if (arg->kind == ALIR_VAL_CONST) {
                                    if (arg->type.base == TYPE_INT) printf("%lld", arg->val.long_long_val);
                                    else if (arg->type.base == TYPE_CLASS && arg->type.class_name && streq_lit(arg->type.class_name, "string")) printf("%s", arg->val.str_val);
                                } else if (arg->kind == ALIR_VAL_TEMP) {
                                    printf("%lld", ctx->registers[arg->temp_id].as.int_val);
                                } else if (arg->kind == ALIR_VAL_VAR) {
                                    printf("%lld", metalir_vm_resolve_var(arg, ctx->module, ctx->vm, ctx->args, ctx->arg_count));
                                } else if (arg->kind == ALIR_VAL_GLOBAL && ctx->module) {
                                    long long ptr = metalir_vm_resolve_var(arg, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                                    if (ptr) printf("%s", (char*)(intptr_t)ptr);
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
                                    if (streq_lit(f->name, inst->op1->val.str_val)) {
                                        target_fn = f;
                                        break;
                                    }
                                    f = f->next;
                                }
                            }

                            if (target_fn && !target_fn->is_extern) {
                                int __na_sz = inst->arg_count > 0 ? inst->arg_count : 1; long long new_args[__na_sz];
                                for (int i = 0; i < inst->arg_count; i++) {
                                    AlirValue *arg = inst->args[i];
                            if (arg->kind == ALIR_VAL_CONST) new_args[i] = arg->val.long_long_val;
                            else if (arg->kind == ALIR_VAL_TEMP) {
                                if (arg->type.ptr_depth > 0 || (arg->type.base == TYPE_CLASS && arg->type.class_name))
                                    new_args[i] = (long long)(intptr_t)ctx->registers[arg->temp_id].as.ptr_val;
                                else
                                    new_args[i] = ctx->registers[arg->temp_id].as.int_val;
                            }
                            else if (arg->kind == ALIR_VAL_VAR) new_args[i] = metalir_vm_resolve_var(arg, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                            else if (arg->kind == ALIR_VAL_GLOBAL) new_args[i] = metalir_vm_resolve_var(arg, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                            else new_args[i] = 0;
                                }
                                long long rc = metalir_vm_execute(ctx->vm, ctx->module, target_fn, ctx->sem_ctx, new_args, inst->arg_count);
                                if (streq_lit(target_fn->name, "Vector_as_int")) {
                                }

                                if (inst->dest) {
                                    ctx->registers[inst->dest->temp_id].as.int_val = rc;
                                }
                            } else {
                                void *func_ptr = dlsym(RTLD_DEFAULT, inst->op1->val.str_val);
                                AlirFunction *ext_func = NULL;
                                if (ctx->module) {
                                    AlirFunction *curr = ctx->module->functions;
                                    while(curr) {
                                        if (streq_lit(curr->name, inst->op1->val.str_val)) { ext_func = curr; break; }
                                        curr = curr->next;
                                    }
                                }

                                if (streq_lit(inst->op1->val.str_val, "malloc")) {
                                    debug_metalir("DLSYM MALLOC: %p\n", func_ptr);
                                }

                                if (func_ptr) {
                                    ffi_cif cif;
                                    int __at_sz = inst->arg_count > 0 ? inst->arg_count : 1; ffi_type *arg_types[__at_sz];
                                    int __av_sz = inst->arg_count > 0 ? inst->arg_count : 1; void *arg_values[__av_sz];
                                    uint64_t arg_data[__av_sz];

                                    for (int i = 0; i < inst->arg_count; i++) {
                                        AlirValue *arg = inst->args[i];
                                        if ((arg->type.base == TYPE_INT || arg->type.base == TYPE_BOOL ||
                                            arg->type.base == TYPE_CHAR || arg->type.base == TYPE_SHORT ||
                                            arg->type.base == TYPE_LONG || arg->type.base == TYPE_LONG_LONG ||
                                            arg->type.base == TYPE_UNSIGNED_INT || arg->type.base == TYPE_UNSIGNED_LONG ||
                                            arg->type.base == TYPE_UNSIGNED_LONG_LONG || arg->type.base == TYPE_UNSIGNED_CHAR) &&
                                            arg->type.ptr_depth == 0) {
                                            arg_types[i] = &ffi_type_sint64;
                                            long long *val = (long long*)&arg_data[i];
                                            if (arg->kind == ALIR_VAL_CONST) *val = arg->val.long_long_val;
                                            else if (arg->kind == ALIR_VAL_TEMP) *val = ctx->registers[arg->temp_id].as.int_val;
                                            else if (arg->kind == ALIR_VAL_VAR) *val = metalir_vm_resolve_var(arg, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                                            else if (arg->kind == ALIR_VAL_GLOBAL && ctx->module) {
                                                long long ptr = metalir_vm_resolve_var(arg, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                                                *val = ptr;
                                            }
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
                                                long long raw = metalir_vm_resolve_var(arg, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                                                if (arg->type.base == TYPE_DOUBLE) memcpy(val, &raw, sizeof(double));
                                                else { float f; memcpy(&f, &raw, sizeof(float)); *(float*)val = f; }
                                            } else if (arg->kind == ALIR_VAL_GLOBAL && ctx->module) {
                                                long long raw = metalir_vm_resolve_var(arg, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                                                if (arg->type.base == TYPE_DOUBLE) memcpy(val, &raw, sizeof(double));
                                                else { float f; memcpy(&f, &raw, sizeof(float)); *(float*)val = f; }
                                            }
                                            arg_values[i] = val;
                                        } else if ((arg->type.base == TYPE_CLASS && arg->type.class_name && streq_lit(arg->type.class_name, "string")) || arg->type.base == TYPE_AUTO || arg->type.ptr_depth > 0) {
                                            arg_types[i] = &ffi_type_pointer;
                                            void **val = (void**)&arg_data[i];
                                            *val = NULL;
                                            if (arg->kind == ALIR_VAL_CONST) *val = (void*)arg->val.str_val;
                                            else if (arg->kind == ALIR_VAL_TEMP) *val = ctx->registers[arg->temp_id].as.ptr_val;
                                            else if (arg->kind == ALIR_VAL_VAR) *val = (void*)(intptr_t)metalir_vm_resolve_var(arg, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                                            else if (arg->kind == ALIR_VAL_GLOBAL && ctx->module) {
                                                long long ptr = metalir_vm_resolve_var(arg, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                                                *val = (void*)(intptr_t)ptr;
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

                                    int prep_status = FFI_OK;
                                    if (ext_func && ext_func->is_varargs) {
                                        prep_status = ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, ext_func->param_count, inst->arg_count, ret_type, arg_types);
                                    } else {
                                        prep_status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, inst->arg_count, ret_type, arg_types);
                                    }
                                    if (prep_status == FFI_OK) {
                                        long long rc_int = 0;
                                        double rc_double = 0;
                                        float rc_float = 0;
                                        void *rc = &rc_int;
                                        if (inst->dest) {
                                            if (inst->dest->type.base == TYPE_DOUBLE) rc = &rc_double;
                                            else if (inst->dest->type.base == TYPE_SINGLE) rc = &rc_float;
                                        }
                                        ffi_call(&cif, func_ptr, rc, arg_values);
                                        if (streq_lit(inst->op1->val.str_val, "malloc")) {
                                            debug_metalir("MALLOC FFI RETURN: %lld\n", rc_int);
                                        }
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
