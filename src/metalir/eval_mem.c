#include "vm_internal.h"
#include <string.h>
#include "common/arena.h"
#include "alir/lvalue.h"
#include "alir/lvalue.h"

static int vm_get_type_align(VarType t) {
    if (t.ptr_depth > 0) return 8;
    if (t.array_size > 0) return 8;
    switch (t.base) {
        case TYPE_BOOL: case TYPE_CHAR: case TYPE_UNSIGNED_CHAR: return 1;
        case TYPE_SHORT: return 2;
        case TYPE_INT: case TYPE_UNSIGNED_INT: case TYPE_SINGLE: case TYPE_ENUM: return 4;
        default: return 8;
    }
}

static int get_struct_field_offset_vm(AlirModule *mod, const char *struct_name, int field_index) {
    AlirStruct *st = alir_find_struct(mod, struct_name);
    if (!st || !st->fields) return field_index * 8;
    
    int byte_offset = 0;
    AlirField *f = st->fields;
    while (f) {
        int align = vm_get_type_align(f->type);
        byte_offset = (byte_offset + align - 1) & ~(align - 1);
        if (f->index == field_index) {
            return byte_offset;
        }
        byte_offset += alir_get_type_size(f->type);
        f = f->next;
    }
    return field_index * 8;
}

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
                        else if (inst->op1->kind == ALIR_VAL_VAR) val = metalir_vm_resolve_var(inst->op1, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                        else if (inst->op1->kind == ALIR_VAL_GLOBAL && ctx->module) {
                            AlirGlobal *g = ctx->module->globals;
                            while(g) {
                                if (streq(g->name, inst->op1->val.str_val)) {
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
                                if (streq(g->name, inst->op2->val.str_val)) {
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
                        else if (inst->op2->kind == ALIR_VAL_VAR) ptr = (void*)(intptr_t)metalir_vm_resolve_var(inst->op2, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                        
                        void *src_ptr = NULL;
                        if (inst->op1->type.base == TYPE_CLASS && inst->op1->type.ptr_depth == 0) {
                            if (inst->op1->kind == ALIR_VAL_TEMP) src_ptr = ctx->registers[inst->op1->temp_id].as.ptr_val;
                            else if (inst->op1->kind == ALIR_VAL_VAR) src_ptr = (void*)(intptr_t)metalir_vm_resolve_var(inst->op1, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                        }
                        
                        if (ptr && src_ptr && (uintptr_t)src_ptr > 0xFFF) {
                            int struct_size = 1024;
                            if (ctx->module && inst->op1->type.class_name) {
                                struct_size = alir_get_struct_size(ctx->module, inst->op1->type.class_name);
                                if (struct_size < 8) struct_size = 8;
                            }
                            memcpy(ptr, src_ptr, struct_size);
                        } else if (ptr) {
                            int size = alir_get_type_size(inst->op1->type);
                            if (size == 1) *(unsigned char*)ptr = (unsigned char)val;
                            else if (size == 2) *(unsigned short*)ptr = (unsigned short)val;
                            else if (size == 4) *(unsigned int*)ptr = (unsigned int)val;
                            else *(unsigned long long*)ptr = (unsigned long long)val;
                        }
                    }
                    break;
                }
                    break;
case ALIR_OP_LOAD: {
                    if (inst->dest && inst->op1) { // dest = value, op1 = ptr
                        void *ptr = NULL;
                        if (inst->op1->kind == ALIR_VAL_TEMP) ptr = ctx->registers[inst->op1->temp_id].as.ptr_val;
                        else if (inst->op1->kind == ALIR_VAL_VAR) ptr = (void*)(intptr_t)metalir_vm_resolve_var(inst->op1, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                        else if (inst->op1->kind == ALIR_VAL_GLOBAL) {
                            VMGlobal *g = ctx->vm->globals;
                            while(g) {
                                if (streq(g->name, inst->op1->val.str_val)) {
                                    ptr = g->ptr_val;
                                    break;
                                }
                                g = g->next;
                            }
                            if (!ptr) {
                                AlirGlobal *ag = ctx->module->globals;
                                while(ag) {
                                    if (streq(ag->name, inst->op1->val.str_val)) {
                                        ptr = (void*)(intptr_t)ag->string_content;
                                        break;
                                    }
                                    ag = ag->next;
                                }
                            }
                        }
                        if (ptr) {
                            if (inst->dest->type.base == TYPE_CLASS && inst->dest->type.ptr_depth == 0) {
                                int struct_size = 1024;
                                if (ctx->module && inst->dest->type.class_name) {
                                    struct_size = alir_get_struct_size(ctx->module, inst->dest->type.class_name);
                                    if (struct_size < 8) struct_size = 8;
                                }
                                void *copy = arena_alloc(ctx->vm->arena, struct_size);
                                memcpy(copy, ptr, struct_size);
                                ctx->registers[inst->dest->temp_id].as.ptr_val = copy;
                            } else {
                                int size = alir_get_type_size(inst->dest->type);
                                long long val = 0;
                                if (size == 1) val = *(unsigned char*)ptr;
                                else if (size == 2) val = *(unsigned short*)ptr;
                                else if (size == 4) val = *(unsigned int*)ptr;
                                else val = *(unsigned long long*)ptr;
                                ctx->registers[inst->dest->temp_id].as.int_val = val;
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
                        else if (inst->op1->kind == ALIR_VAL_VAR) base_ptr = (void*)(intptr_t)metalir_vm_resolve_var(inst->op1, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                        else if (inst->op1->kind == ALIR_VAL_GLOBAL) {
                            VMGlobal *g = ctx->vm->globals;
                            while(g) {
                                if (streq(g->name, inst->op1->val.str_val)) { base_ptr = g->ptr_val; break; }
                                g = g->next;
                            }
                        }
                        
                        long long offset = 0;
                        if (inst->op2->kind == ALIR_VAL_CONST) offset = inst->op2->val.long_long_val;
                        else if (inst->op2->kind == ALIR_VAL_TEMP) offset = ctx->registers[inst->op2->temp_id].as.int_val;
                        else if (inst->op2->kind == ALIR_VAL_VAR) offset = metalir_vm_resolve_var(inst->op2, ctx->module, ctx->vm, ctx->args, ctx->arg_count);
                        
                        // Treat offset as field index if class type, else array index
                        int byte_offset = 0;
                        VarType bt = inst->op1->type;
                        if (bt.ptr_depth > 0) bt.ptr_depth--;
                        else if (bt.array_size > 0) bt.array_size = 0;

                        if (bt.base == TYPE_CLASS && bt.class_name && bt.ptr_depth == 0 && bt.array_size == 0) {
                            byte_offset = get_struct_field_offset_vm(ctx->module, bt.class_name, (int)offset);
                        } else {
                            VarType elem_type = inst->dest->type;
                            if (elem_type.ptr_depth > 0) elem_type.ptr_depth--;
                            int elem_size = alir_get_type_size(elem_type);
                            byte_offset = offset * elem_size;
                        }
                        
                        if (base_ptr) {
                            ctx->registers[inst->dest->temp_id].as.ptr_val = (void*)((char*)base_ptr + byte_offset);
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
