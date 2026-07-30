#include "alir.h"
#include "common.h"

AlirInst* mk_inst(AlirModule *mod, AlirOpcode op, AlirValue *dest, AlirValue *op1, AlirValue *op2) {
    AlirInst *i = alir_alloc(mod, sizeof(AlirInst));
    i->op = op;
    i->dest = dest;
    i->op1 = op1;
    i->op2 = op2;
    i->line = 0;
    i->col = 0;
    return i;
}

void emit(AlirCtx *ctx, AlirInst *i) {
    if (!ctx->current_block) return;
    if (i) {
        i->line = ctx->current_line;
        i->col = ctx->current_col;
    }
    if (ctx->current_func && streq(ctx->current_func->name, "main")) {
         debug_any("func=%s op=%d dest_kind=%d dest_type_base=%d op1_kind=%d op1_type_base=%d\n",
             ctx->current_func->name,
             (int)(i ? i->op : 0),
             (int)(i && i->dest ? i->dest->kind : 0),
             (int)(i && i->dest ? i->dest->type.base : 0),
             (int)(i && i->op1 ? i->op1->kind : 0),
             (int)(i && i->op1 ? i->op1->type.base : 0));
    }
    alir_append_inst(ctx->current_block, i);
}

AlirValue* new_temp(AlirCtx *ctx, VarType t) {
    return alir_val_temp(ctx->module, t, ctx->temp_counter++);
}

AlirValue* promote(AlirCtx *ctx, AlirValue *v, VarType target) {
    // Basic Promotion Logic: Check base types
    if (v->type.base == target.base && v->type.ptr_depth == target.ptr_depth) return v;

    AlirValue *dest = new_temp(ctx, target);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_CAST, dest, v, NULL));
    return dest;
}

// Symbol Table (IR Level: Maps names to Allocas/Registers)
void alir_add_symbol(AlirCtx *ctx, const char *name, AlirValue *ptr, VarType t) {
    AlirSymbol *s = alir_alloc(ctx->module, sizeof(AlirSymbol));
    s->name = alir_strdup(ctx->module, name);
    s->ptr = ptr;
    s->type = t;
    s->next = ctx->symbols;
    ctx->symbols = s;
    hashmap_put(&ctx->symbol_map, s->name, s);
}

AlirSymbol* alir_find_symbol(AlirCtx *ctx, const char *name) {
    AlirSymbol *s = (AlirSymbol*)hashmap_get(&ctx->symbol_map, name);
    if (s) return s;

    // Fallback to linked list in case map wasn't initialized properly in some legacy code paths
    s = ctx->symbols;
    while(s) {
        if (streq(s->name, name)) return s;
        s = s->next;
    }
    return NULL;
}

// what is new lower object
AlirValue* alir_lower_new_object(AlirCtx *ctx, const char *class_name, ASTNode *args) {
    // Verify struct exists in IR
    AlirStruct *st = alir_find_struct(ctx->module, class_name);
    if (!st) return NULL;

    // 1. Sizeof
    AlirValue *size_val = alir_const_int(ctx->module, 8);

    // 2. Alloc Stack (Alloca)
    AlirValue *raw_mem = new_temp(ctx, (VarType){TYPE_CHAR, 1}); // char*
    emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, raw_mem, size_val, NULL));

    // 3. Bitcast to Class*
    VarType cls_ptr_type = {TYPE_CLASS, 1, alir_strdup(ctx->module, class_name)};
    AlirValue *obj_ptr = new_temp(ctx, cls_ptr_type);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_BITCAST, obj_ptr, raw_mem, NULL));

    // 4. Call Constructor or Implicit Member Initialization
    SemSymbol *sym = ctx->sem ? sem_symbol_lookup(ctx->sem, class_name, NULL) : NULL;
    int ctor_exists = 0;
    const char *ctor_name = class_name;
    if (sym && sym->inner_scope) {
         SemSymbol *constructor = sym->inner_scope->symbols;
         while (constructor) {
             if (streq(constructor->name, class_name) || streq(constructor->name, "init")) {
                 ctor_exists = 1;
                 if (constructor->mangled_name) ctor_name = constructor->mangled_name;
                 break;
             }
             constructor = constructor->next;
         }
    }

    if (ctor_exists) {
        AlirInst *call_init = mk_inst(ctx->module, ALIR_OP_CALL, NULL, alir_val_global(ctx->module, ctor_name, (VarType){TYPE_VOID, 0, NULL}), NULL);
        int arg_count = 0; ASTNode *a = args; while(a) { arg_count++; a=a->next; }
        call_init->arg_count = arg_count + 1;
        call_init->args = alir_alloc(ctx->module, sizeof(AlirValue*) * (arg_count + 1));
        call_init->args[0] = obj_ptr; // THIS pointer
        int i = 1; a = args;
        while(a) {
            call_init->args[i++] = alir_gen_expr(ctx, a);
            a = a->next;
        }
        call_init->dest = new_temp(ctx, (VarType){TYPE_VOID, 0});
        emit(ctx, call_init);
    } else {
        // Implicit initialization (structural assignment)
        int idx = 0;
        ASTNode *a = args;
        while(a) {
            AlirValue *val = alir_gen_expr(ctx, a);
            if (val) {
                // Get pointer to field
                AlirValue *field_ptr = new_temp(ctx, (VarType){TYPE_AUTO, 1, NULL});
                emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, field_ptr, obj_ptr, alir_const_int(ctx->module, idx)));
                // Store value: op1 is VALUE, op2 is DEST POINTER
                emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, val, field_ptr));
            }
            idx++;
            a = a->next;
        }
    }

    VarType cls_val_type = {TYPE_CLASS, 0, alir_strdup(ctx->module, class_name)};
    AlirValue *obj_val = new_temp(ctx, cls_val_type);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, obj_val, obj_ptr, NULL));

    return obj_val;
}
