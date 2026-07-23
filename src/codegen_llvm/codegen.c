#include "../../include/codegen_llvm/codegen.h"
#include "../../include/common/hashmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

CodegenCtx* codegen_init(AlirModule *mod) {
    CodegenCtx *ctx = calloc(1, sizeof(CodegenCtx));
    ctx->alir_mod = mod;
    ctx->llvm_ctx = LLVMContextCreate();
    ctx->llvm_mod = LLVMModuleCreateWithNameInContext(mod->name ? mod->name : "alick_module", ctx->llvm_ctx);
    ctx->builder = LLVMCreateBuilderInContext(ctx->llvm_ctx);

    ctx->arena = mod->compiler_ctx ? mod->compiler_ctx->arena : NULL;

    // Initialize resolution maps
    hashmap_init(&ctx->value_map, ctx->arena, 256);
    hashmap_init(&ctx->block_map, ctx->arena, 256);
    hashmap_init(&ctx->struct_map, ctx->arena, 64);
    hashmap_init(&ctx->func_map, ctx->arena, 64);
    hashmap_init(&ctx->func_type_map, ctx->arena, 64);

    return ctx;
}

void codegen_dispose(CodegenCtx *ctx) {
    if (!ctx) return;
    LLVMDisposeBuilder(ctx->builder);

    // Note: To preserve LLVMModule for execution/JIT, we only clean up the builder.
    // The module and LLVMContext will need to be disposed later by the driver.
    free(ctx);
}

LLVMTypeRef get_llvm_type(CodegenCtx *ctx, VarType t) {
    LLVMTypeRef base = NULL;
    if (t.ptr_depth > 0 || t.is_func_ptr) {
        base = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
    } else {
        switch (t.base) {
            case TYPE_VOID: base = LLVMVoidTypeInContext(ctx->llvm_ctx); break;
        case TYPE_ERROR: base = LLVMInt32TypeInContext(ctx->llvm_ctx); break;
        case TYPE_INT: base = LLVMInt32TypeInContext(ctx->llvm_ctx); break;
        case TYPE_SHORT: base = LLVMInt16TypeInContext(ctx->llvm_ctx); break;
        case TYPE_LONG:
        case TYPE_LONG_LONG: base = LLVMInt64TypeInContext(ctx->llvm_ctx); break;
        case TYPE_CHAR: base = LLVMInt8TypeInContext(ctx->llvm_ctx); break;
        case TYPE_BOOL: base = LLVMInt1TypeInContext(ctx->llvm_ctx); break;
        case TYPE_SINGLE: base = LLVMFloatTypeInContext(ctx->llvm_ctx); break;
        case TYPE_DOUBLE:
        case TYPE_LONG_DOUBLE: base = LLVMDoubleTypeInContext(ctx->llvm_ctx); break;

        case TYPE_CLASS: {
            if (t.class_name) {
                base = hashmap_get(&ctx->struct_map, t.class_name);
                if (!base) {
                    base = LLVMStructCreateNamed(ctx->llvm_ctx, t.class_name);
                    hashmap_put(&ctx->struct_map, t.class_name, base);
                }
            } else {
                base = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
            }
            break;
        }
        case TYPE_ENUM: base = LLVMInt32TypeInContext(ctx->llvm_ctx); break;

        // TODO vector, hashmap, auto, unknown
        default: base = LLVMInt32TypeInContext(ctx->llvm_ctx); break;
        }
    }

    if (t.array_size > 0) {
        base = LLVMArrayType(base, t.array_size);
    }

    if (t.is_tainted) {
        LLVMTypeRef elements[] = {
            LLVMInt32TypeInContext(ctx->llvm_ctx),  // i32 error_id
            base                                    // actual value
        };
        if (t.base == TYPE_VOID && t.ptr_depth == 0) {
            return LLVMStructTypeInContext(ctx->llvm_ctx, elements, 1, 0);
        }
        return LLVMStructTypeInContext(ctx->llvm_ctx, elements, 2, 0);
    }

    return base;
}

void set_llvm_value(CodegenCtx *ctx, AlirValue *v, LLVMValueRef llvm_val) {
    if (!v) return;
    if (v->kind == ALIR_VAL_TEMP) {
        if (v->temp_id < ctx->max_temps) {
            ctx->temps[v->temp_id] = llvm_val;
        }
    } else if (v->kind == ALIR_VAL_VAR) {
        hashmap_put(&ctx->value_map, v->val.str_val, llvm_val);
    }
}

LLVMValueRef get_llvm_value(CodegenCtx *ctx, AlirValue *v) {
    if (!v) return NULL;

    switch (v->kind) {
        case ALIR_VAL_CONST: {
            LLVMTypeRef ty = get_llvm_type(ctx, v->type);
            if (v->type.base == TYPE_SINGLE) {
                return LLVMConstReal(ty, v->val.single_val);
            } else if (v->type.base == TYPE_DOUBLE) {
                return LLVMConstReal(ty, v->val.double_val);
            } else if (v->type.base == TYPE_LONG) {
                return LLVMConstInt(ty, v->val.long_val, !v->type.is_unsigned);
            } else {
                return LLVMConstInt(ty, v->val.int_val, !v->type.is_unsigned);
            }
        }
        case ALIR_VAL_TEMP:
            if (v->temp_id < ctx->max_temps) return ctx->temps[v->temp_id];
            return NULL;

        case ALIR_VAL_VAR:
            return hashmap_get(&ctx->value_map, v->val.str_val);

        case ALIR_VAL_GLOBAL: {
            // First check if it's a global variable
            LLVMValueRef glob = LLVMGetNamedGlobal(ctx->llvm_mod, v->val.str_val);
            if (!glob) {
                // If not found, it might be a function masquerading as a global value
                glob = LLVMGetNamedFunction(ctx->llvm_mod, v->val.str_val);
            }

            // We do not bitcast here anymore, because glob is already of the correct struct pointer type.
            // Wait, if it expects the struct by value, maybe we should load it?
            // In LLVM, ALIR_VAL_GLOBAL returning a pointer is standard.
            return glob;
        }
        case ALIR_VAL_TYPE:
            // SIZEOF needs the actual LLVMType, not a value. We handle this inside translation.
            // TODO: needs a catch here!
            return NULL;
        case ALIR_VAL_LABEL:
            // TODO: needs a catch here!
            return NULL;
        // TODO alir_val_void, int, float, string
        default:
          return NULL;
    }
}

LLVMModuleRef codegen_generate(CodegenCtx *ctx) {
    // 1. Pre-declare Structs (Opaque pass to resolve cross references)
    AlirStruct *st = ctx->alir_mod->structs;
    while (st) {
        LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx->llvm_ctx, st->name);
        hashmap_put(&ctx->struct_map, st->name, struct_ty);
        st = st->next;
    }

    // 1.5. Populate Struct Bodies
    st = ctx->alir_mod->structs;
    while (st) {
        if (st->field_count > 0) {
            LLVMTypeRef *field_tys = malloc(sizeof(LLVMTypeRef) * st->field_count);
            AlirField *f = st->fields;
            while(f) {
                field_tys[f->index] = get_llvm_type(ctx, f->type);
                f = f->next;
            }
            LLVMTypeRef struct_ty = hashmap_get(&ctx->struct_map, st->name);

            if (st->is_union) {
                LLVMTargetDataRef td = LLVMCreateTargetData("");
                unsigned long long max_size = 0;
                unsigned long long max_align = 0;

                for (int i = 0; i < st->field_count; i++) {
                    unsigned long long sz = LLVMABISizeOfType(td, field_tys[i]);
                    unsigned long long al = LLVMABIAlignmentOfType(td, field_tys[i]);
                    if (sz > max_size) max_size = sz;
                    if (al > max_align) max_align = al;
                }

                LLVMTypeRef best_align_ty = NULL;
                unsigned long long best_align_sz = 0;
                for (int i = 0; i < st->field_count; i++) {
                    unsigned long long sz = LLVMABISizeOfType(td, field_tys[i]);
                    unsigned long long al = LLVMABIAlignmentOfType(td, field_tys[i]);
                    if (al == max_align) {
                        if (sz >= best_align_sz) {
                            best_align_sz = sz;
                            best_align_ty = field_tys[i];
                        }
                    }
                }

                // If there's no struct fields, default to something safe
                if (!best_align_ty) {
                    best_align_ty = LLVMInt8TypeInContext(ctx->llvm_ctx);
                    max_align = 1;
                    max_size = 1;
                    best_align_sz = 1;
                }

                unsigned long long aligned_max_size = max_align > 0 ? ((max_size + max_align - 1) / max_align * max_align) : max_size;
                unsigned long long padding = aligned_max_size > best_align_sz ? (aligned_max_size - best_align_sz) : 0;

                if (padding > 0) {
                    LLVMTypeRef union_body[2];
                    union_body[0] = best_align_ty;
                    union_body[1] = LLVMArrayType(LLVMInt8TypeInContext(ctx->llvm_ctx), padding);
                    LLVMStructSetBody(struct_ty, union_body, 2, 0);
                } else {
                    LLVMTypeRef union_body[1];
                    union_body[0] = best_align_ty;
                    LLVMStructSetBody(struct_ty, union_body, 1, 0);
                }

                LLVMDisposeTargetData(td);
            } else {
                LLVMStructSetBody(struct_ty, field_tys, st->field_count, 0);
            }
            free(field_tys);
        }
        st = st->next;
    }

    // 2. Global Strings / Variables
    AlirGlobal *g = ctx->alir_mod->globals;
    while (g) {
        if (g->string_content) {
            LLVMValueRef init_str = LLVMConstStringInContext(ctx->llvm_ctx, g->string_content, strlen(g->string_content), 0);

            if (g->type.base == TYPE_CLASS && g->type.class_name && strcmp(g->type.class_name, "string") == 0) {
                LLVMTypeRef str_array_ty = LLVMTypeOf(init_str);
                char internal_name[256];
                snprintf(internal_name, sizeof(internal_name), "%s_data", g->name);
                LLVMValueRef internal_var = LLVMAddGlobal(ctx->llvm_mod, str_array_ty, internal_name);
                LLVMSetInitializer(internal_var, init_str);
                LLVMSetLinkage(internal_var, LLVMPrivateLinkage);
                LLVMSetGlobalConstant(internal_var, 1);

                LLVMTypeRef ptr_ty = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
                LLVMValueRef ptr_val = LLVMConstPointerCast(internal_var, ptr_ty);

                LLVMValueRef len_val = LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), g->string_content ? strlen(g->string_content) : 0, 0);

                LLVMTypeRef class_type = get_llvm_type(ctx, g->type);
                if (class_type && LLVMIsOpaqueStruct(class_type)) {
                    LLVMTypeRef types[] = { LLVMInt32TypeInContext(ctx->llvm_ctx), ptr_ty };
                    LLVMStructSetBody(class_type, types, 2, 0);
                }

                LLVMValueRef struct_vals[] = { len_val, ptr_val };
                LLVMValueRef class_init = LLVMConstNamedStruct(class_type, struct_vals, 2);
                if (!class_init) class_init = LLVMConstStructInContext(ctx->llvm_ctx, struct_vals, 2, 0);

                LLVMValueRef global_var = LLVMAddGlobal(ctx->llvm_mod, LLVMTypeOf(class_init), g->name);
                LLVMSetInitializer(global_var, class_init);
                LLVMSetLinkage(global_var, LLVMPrivateLinkage);
                LLVMSetGlobalConstant(global_var, 1);
            } else {
                LLVMTypeRef str_ty = LLVMTypeOf(init_str);
                LLVMValueRef global_var = LLVMAddGlobal(ctx->llvm_mod, str_ty, g->name);
                LLVMSetInitializer(global_var, init_str);
                LLVMSetLinkage(global_var, LLVMPrivateLinkage);
                LLVMSetGlobalConstant(global_var, 1);
            }
        }
        g = g->next;
    }

    // 3. Function Prototypes (Declarations)
    AlirFunction *func = ctx->alir_mod->functions;
    while (func) {
        VarType real_ret_ty = func->ret_type;
        if (func->is_extern) real_ret_ty.is_tainted = 0;
        LLVMTypeRef ret_ty = get_llvm_type(ctx, real_ret_ty);
        LLVMTypeRef *param_tys = NULL;

        if (func->param_count > 0) {
            param_tys = malloc(sizeof(LLVMTypeRef) * func->param_count);
            AlirParam *p = func->params;
            int i = 0;
            while(p) {
                VarType p_ty = p->type;
                if (func->is_extern) p_ty.is_tainted = 0;
                if (p_ty.array_size > 0) { p_ty.array_size = 0; p_ty.ptr_depth++; } // Parameter decay
                param_tys[i++] = get_llvm_type(ctx, p_ty);
                p = p->next;
            }
        }

        LLVMTypeRef func_ty = LLVMFunctionType(ret_ty, param_tys, func->param_count, func->is_varargs);
        LLVMValueRef llvm_func = LLVMAddFunction(ctx->llvm_mod, func->name, func_ty);

        if (func->cconv) {
            if (strcmp(func->cconv, "stdcall") == 0 || strcmp(func->cconv, "\"stdcall\"") == 0) {
                LLVMSetFunctionCallConv(llvm_func, 64); // LLVMX86StdcallCallConv
            } else if (strcmp(func->cconv, "fastcall") == 0 || strcmp(func->cconv, "\"fastcall\"") == 0) {
                LLVMSetFunctionCallConv(llvm_func, 65); // LLVMX86FastcallCallConv
            } else if (strcmp(func->cconv, "cdecl") == 0 || strcmp(func->cconv, "\"cdecl\"") == 0) {
                LLVMSetFunctionCallConv(llvm_func, 0); // LLVMCCallConv
            }
        }

        hashmap_put(&ctx->func_map, func->name, llvm_func);
        hashmap_put(&ctx->func_type_map, func->name, func_ty);

        if (param_tys) free(param_tys);
        func = func->next;
    }

    // 4. Function Bodies
    func = ctx->alir_mod->functions;
    while (func) {
        if (func->block_count == 0) { func = func->next; continue; }

        LLVMValueRef llvm_func = hashmap_get(&ctx->func_map, func->name);

        // Scan instructions to find max needed `temps` length
        ctx->max_temps = 0;
        AlirBlock *b = func->blocks;
        while(b) {
            AlirInst *i = b->head;
            while(i) {
                if (i->dest && i->dest->kind == ALIR_VAL_TEMP && i->dest->temp_id >= ctx->max_temps) {
                    ctx->max_temps = i->dest->temp_id + 1;
                }
                i = i->next;
            }
            b = b->next;
        }

        ctx->temps = calloc(ctx->max_temps, sizeof(LLVMValueRef));

        // Map native parameter locals to value map (e.g. `p0`, `p1` injected by ALIR generator)
        AlirParam *p = func->params;
        int p_idx = 0;
        while(p) {
            char pname[16]; snprintf(pname, sizeof(pname), "p%d", p_idx);
            LLVMValueRef param_val = LLVMGetParam(llvm_func, p_idx);
            hashmap_put(&ctx->value_map, pname, param_val);
            p_idx++;
            p = p->next;
        }

        // Create Basic Blocks
        b = func->blocks;
        while(b) {
            LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, llvm_func, b->label);
            hashmap_put(&ctx->block_map, b->label, bb);
            b = b->next;
        }

        // Evaluate Instructions
        b = func->blocks;
        while(b) {
            LLVMBasicBlockRef bb = hashmap_get(&ctx->block_map, b->label);
            LLVMPositionBuilderAtEnd(ctx->builder, bb);

            AlirInst *inst = b->head;
            while(inst) {
                translate_inst(ctx, inst);
                inst = inst->next;
            }

            // Safety Net: Terminate Basic Block if implicit
            if (!LLVMGetBasicBlockTerminator(bb)) {
                if (func->ret_type.base == TYPE_VOID) {
                    LLVMBuildRetVoid(ctx->builder);
                } else {
                    printf("Unreachable!\n");
                    LLVMBuildUnreachable(ctx->builder);
                }
            }

            b = b->next;
        }

        free(ctx->temps);
        ctx->temps = NULL;
        func = func->next;
    }

    // Verify Module Integrity Check (Optional safety)
    char *err_msg = NULL;
    LLVMVerifyModule(ctx->llvm_mod, LLVMPrintMessageAction, &err_msg);
    if (err_msg) {
        LLVMDisposeMessage(err_msg);
    }

    return ctx->llvm_mod;
}

void codegen_emit_to_file(CodegenCtx *ctx, const char *filename) {
    if (!ctx || !ctx->llvm_mod) return;
    char *err_msg = NULL;
    LLVMPrintModuleToFile(ctx->llvm_mod, filename, &err_msg);
    if (err_msg) {
        fprintf(stderr, "LLVM File Emission Error: %s\n", err_msg);
        LLVMDisposeMessage(err_msg);
    }
}

void codegen_print(CodegenCtx *ctx) {
    if (!ctx || !ctx->llvm_mod) return;
    char *ir = LLVMPrintModuleToString(ctx->llvm_mod);
    printf("%s", ir);
    LLVMDisposeMessage(ir);
}
