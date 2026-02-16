#include "alir.h"
#include <stdlib.h>
#include <string.h>

// --- HELPERS ---

AlirInst* mk_inst(AlirOpcode op, AlirValue *dest, AlirValue *op1, AlirValue *op2) {
    AlirInst *i = calloc(1, sizeof(AlirInst));
    i->op = op;
    i->dest = dest;
    i->op1 = op1;
    i->op2 = op2;
    return i;
}

void emit(AlirCtx *ctx, AlirInst *i) {
    if (!ctx->current_block) return;
    alir_append_inst(ctx->current_block, i);
}

AlirValue* new_temp(AlirCtx *ctx, VarType t) {
    return alir_val_temp(t, ctx->temp_counter++);
}

AlirValue* promote(AlirCtx *ctx, AlirValue *v, VarType target) {
    if (v->type.base == target.base) return v;
    AlirValue *dest = new_temp(ctx, target);
    emit(ctx, mk_inst(ALIR_OP_CAST, dest, v, NULL));
    return dest;
}

// Symbol Table
void alir_add_symbol(AlirCtx *ctx, const char *name, AlirValue *ptr, VarType t) {
    AlirSymbol *s = calloc(1, sizeof(AlirSymbol));
    s->name = strdup(name);
    s->ptr = ptr;
    s->type = t;
    s->next = ctx->symbols;
    ctx->symbols = s;
}

AlirSymbol* alir_find_symbol(AlirCtx *ctx, const char *name) {
    AlirSymbol *s = ctx->symbols;
    while(s) {
        if (strcmp(s->name, name) == 0) return s;
        s = s->next;
    }
    return NULL;
}

// Loop Stack
void push_loop(AlirCtx *ctx, AlirBlock *cont, AlirBlock *brk) {
    AlirCtx *node = malloc(sizeof(AlirCtx));
    node->loop_continue = ctx->loop_continue;
    node->loop_break = ctx->loop_break;
    node->loop_parent = ctx->loop_parent;
    ctx->loop_parent = node;
    ctx->loop_continue = cont;
    ctx->loop_break = brk;
}

void pop_loop(AlirCtx *ctx) {
    if (!ctx->loop_parent) return;
    AlirCtx *node = ctx->loop_parent;
    ctx->loop_continue = node->loop_continue;
    ctx->loop_break = node->loop_break;
    ctx->loop_parent = node->loop_parent;
    free(node);
}

// Helper: Calculate type of expression (rudimentary)
VarType alir_calc_type(AlirCtx *ctx, ASTNode *node) {
    VarType vt = {TYPE_INT, 0};
    if (node->type == NODE_LITERAL) vt = ((LiteralNode*)node)->var_type;
    if (node->type == NODE_VAR_REF) {
        AlirSymbol *s = alir_find_symbol(ctx, ((VarRefNode*)node)->name);
        if (s) vt = s->type;
    }
    // Attempt rudimentary inference for member access
    if (node->type == NODE_MEMBER_ACCESS) {
         // This is weak, requires proper symbol table or type info from parser
         MemberAccessNode *ma = (MemberAccessNode*)node;
         VarType obj_t = alir_calc_type(ctx, ma->object);
         if (obj_t.class_name) {
              // We could look up field type in registry if we stored it
         }
    }
    return vt;
}

// --- LOWERING HELPER: Register Class Layout ---
void alir_scan_and_register_classes(AlirCtx *ctx, ASTNode *root) {
    ASTNode *curr = root;
    while(curr) {
        if (curr->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)curr;
            
            AlirField *head = NULL;
            AlirField **tail = &head;
            int idx = 0;

            // Flatten inheritance if necessary, simplified scan local members
            ASTNode *mem = cn->members;
            while(mem) {
                if (mem->type == NODE_VAR_DECL) {
                    VarDeclNode *vd = (VarDeclNode*)mem;
                    AlirField *f = calloc(1, sizeof(AlirField));
                    f->name = strdup(vd->name);
                    f->type = vd->var_type;
                    f->index = idx++;
                    
                    *tail = f;
                    tail = &f->next;
                }
                mem = mem->next;
            }
            
            alir_register_struct(ctx->module, cn->name, head);
        } else if (curr->type == NODE_NAMESPACE) {
            // Recursive scan for namespaces if needed
             alir_scan_and_register_classes(ctx, ((NamespaceNode*)curr)->body);
        }
        curr = curr->next;
    }
}

// --- LOWERING CONSTRUCTOR (The Magic) ---
// Transforms: new ClassName() -> sizeof, alloc, cast, init
AlirValue* alir_lower_new_object(AlirCtx *ctx, const char *class_name, ASTNode *args) {
    AlirStruct *st = alir_find_struct(ctx->module, class_name);
    if (!st) return NULL; 

    // 1. Sizeof
    AlirValue *size_val = new_temp(ctx, (VarType){TYPE_INT, 0});
    AlirInst *i_size = mk_inst(ALIR_OP_SIZEOF, size_val, alir_val_type(class_name), NULL);
    emit(ctx, i_size);

    // 2. Alloc Heap (Malloc)
    AlirValue *raw_mem = new_temp(ctx, (VarType){TYPE_CHAR, 1}); // char*
    emit(ctx, mk_inst(ALIR_OP_ALLOC_HEAP, raw_mem, size_val, NULL));

    // 3. Bitcast to Class*
    VarType cls_ptr_type = {TYPE_CLASS, 1, strdup(class_name)};
    AlirValue *obj_ptr = new_temp(ctx, cls_ptr_type);
    emit(ctx, mk_inst(ALIR_OP_BITCAST, obj_ptr, raw_mem, NULL));

    // 4. Call Constructor (Init)
    // Assumption: Init function is named "ClassName" or "ClassName_init" 
    // The parser/codegen usually mangles this. For ALIR, let's assume direct name match for now
    // or assume the user calls "ClassName(...)".
    
    // We construct a call instruction to the initializer
    AlirInst *call_init = mk_inst(ALIR_OP_CALL, NULL, alir_val_var(class_name), NULL);
    
    // Count args + 1 (for 'this')
    int arg_count = 0; ASTNode *a = args; while(a) { arg_count++; a=a->next; }
    call_init->arg_count = arg_count + 1;
    call_init->args = malloc(sizeof(AlirValue*) * (arg_count + 1));
    
    call_init->args[0] = obj_ptr; // THIS pointer
    
    int i = 1; a = args;
    while(a) {
        call_init->args[i++] = alir_gen_expr(ctx, a);
        a = a->next;
    }
    
    // The init function (constructor) typically returns void, but we return the object pointer
    call_init->dest = new_temp(ctx, (VarType){TYPE_VOID, 0}); // Dummy dest for void call
    emit(ctx, call_init);
    
    return obj_ptr;
}


// --- L-VALUE GENERATION (Address Calculation) ---

AlirValue* alir_gen_addr(AlirCtx *ctx, ASTNode *node) {
    if (node->type == NODE_VAR_REF) {
        VarRefNode *vn = (VarRefNode*)node;
        AlirSymbol *sym = alir_find_symbol(ctx, vn->name);
        if (sym) return sym->ptr;
        return alir_val_var(vn->name); // Global fallback
    }
    
    if (node->type == NODE_MEMBER_ACCESS) {
        MemberAccessNode *ma = (MemberAccessNode*)node;
        AlirValue *base_ptr = alir_gen_addr(ctx, ma->object);
        if (!base_ptr) base_ptr = alir_gen_expr(ctx, ma->object); // Handle pointer returns

        VarType obj_t = alir_calc_type(ctx, ma->object);
        
        if (obj_t.class_name) {
            // SMART LOOKUP: Use Registry
            int idx = alir_get_field_index(ctx->module, obj_t.class_name, ma->member_name);
            if (idx != -1) {
                // GEP: base + 0 (deref if pointer) + idx
                // Result type is abstract pointer (int*) for ALIR
                AlirValue *res = new_temp(ctx, (VarType){TYPE_INT, 1}); 
                emit(ctx, mk_inst(ALIR_OP_GET_PTR, res, base_ptr, alir_const_int(idx)));
                return res;
            }
        }
    }
    
    if (node->type == NODE_ARRAY_ACCESS) {
        ArrayAccessNode *aa = (ArrayAccessNode*)node;
        AlirValue *base_ptr = alir_gen_addr(ctx, aa->target);
        AlirValue *index = alir_gen_expr(ctx, aa->index);
        
        AlirValue *res = new_temp(ctx, (VarType){TYPE_INT, 1});
        emit(ctx, mk_inst(ALIR_OP_GET_PTR, res, base_ptr, index));
        return res;
    }
    
    return NULL;
}

AlirValue* alir_gen_literal(AlirCtx *ctx, LiteralNode *ln) {
    if (ln->var_type.base == TYPE_INT) return alir_const_int(ln->val.int_val);
    if (ln->var_type.base == TYPE_FLOAT) return alir_const_float(ln->val.double_val);
    if (ln->var_type.base == TYPE_STRING) {
        // Extract string to global constant pool
        return alir_module_add_string_literal(ctx->module, ln->val.str_val, ctx->str_counter++);
    }
    return alir_const_int(0);
}

AlirValue* alir_gen_var_ref(AlirCtx *ctx, VarRefNode *vn) {
    AlirValue *ptr = alir_gen_addr(ctx, (ASTNode*)vn);
    AlirSymbol *sym = alir_find_symbol(ctx, vn->name);
    VarType t = sym ? sym->type : (VarType){TYPE_INT, 0};
    
    AlirValue *val = new_temp(ctx, t);
    emit(ctx, mk_inst(ALIR_OP_LOAD, val, ptr, NULL));
    return val;
}

AlirValue* alir_gen_access(AlirCtx *ctx, ASTNode *node) {
    AlirValue *ptr = alir_gen_addr(ctx, node);
    AlirValue *val = new_temp(ctx, (VarType){TYPE_INT, 0}); // Simplified type inference
    emit(ctx, mk_inst(ALIR_OP_LOAD, val, ptr, NULL));
    return val;
}

AlirValue* alir_gen_binary_op(AlirCtx *ctx, BinaryOpNode *bn) {
    AlirValue *l = alir_gen_expr(ctx, bn->left);
    AlirValue *r = alir_gen_expr(ctx, bn->right);
    
    // Auto-promote
    if (l->type.base == TYPE_FLOAT || r->type.base == TYPE_FLOAT) {
        l = promote(ctx, l, (VarType){TYPE_FLOAT, 0});
        r = promote(ctx, r, (VarType){TYPE_FLOAT, 0});
    }

    AlirOpcode op = ALIR_OP_ADD;
    switch(bn->op) {
        case TOKEN_PLUS: op = ALIR_OP_ADD; break;
        case TOKEN_MINUS: op = ALIR_OP_SUB; break;
        case TOKEN_STAR: op = ALIR_OP_MUL; break;
        case TOKEN_SLASH: op = ALIR_OP_DIV; break;
        case TOKEN_EQ: op = ALIR_OP_EQ; break;
        case TOKEN_LT: op = ALIR_OP_LT; break;
        // ... (Others mapped similarly)
    }
    
    AlirValue *dest = new_temp(ctx, l->type);
    emit(ctx, mk_inst(op, dest, l, r));
    return dest;
}

// Standard call generation (exposed via forward decl if needed)
AlirValue* alir_gen_call_std(AlirCtx *ctx, CallNode *cn) {
    AlirInst *call = mk_inst(ALIR_OP_CALL, NULL, alir_val_var(cn->name), NULL);
    
    // Count args
    int count = 0; ASTNode *a = cn->args; while(a) { count++; a=a->next; }
    call->arg_count = count;
    call->args = malloc(sizeof(AlirValue*) * count);
    
    int i = 0; a = cn->args;
    while(a) {
        call->args[i++] = alir_gen_expr(ctx, a);
        a = a->next;
    }
    
    AlirValue *dest = new_temp(ctx, (VarType){TYPE_INT, 0}); 
    call->dest = dest;
    emit(ctx, call);
    return dest;
}

// Smart wrapper for call generation that detects Constructors
AlirValue* alir_gen_call(AlirCtx *ctx, CallNode *cn) {
    // Check if this is a class instantiation
    if (alir_find_struct(ctx->module, cn->name)) {
        return alir_lower_new_object(ctx, cn->name, cn->args);
    }
    return alir_gen_call_std(ctx, cn);
}

AlirValue* alir_gen_method_call(AlirCtx *ctx, MethodCallNode *mc) {
    // 1. Calculate 'this' pointer
    AlirValue *this_ptr = alir_gen_addr(ctx, mc->object);
    if (!this_ptr) this_ptr = alir_gen_expr(ctx, mc->object); 

    // 2. Resolve Name (Mangle: Class_Method)
    VarType obj_t = alir_calc_type(ctx, mc->object);
    char func_name[256];
    if (obj_t.class_name) snprintf(func_name, 256, "%s_%s", obj_t.class_name, mc->method_name);
    else snprintf(func_name, 256, "%s", mc->method_name);

    AlirInst *call = mk_inst(ALIR_OP_CALL, NULL, alir_val_var(func_name), NULL);
    
    // 3. Prepare Args (prepend 'this')
    int count = 0; ASTNode *a = mc->args; while(a) { count++; a=a->next; }
    call->arg_count = count + 1;
    call->args = malloc(sizeof(AlirValue*) * (count + 1));
    
    call->args[0] = this_ptr;
    int i = 1; a = mc->args;
    while(a) {
        call->args[i++] = alir_gen_expr(ctx, a);
        a = a->next;
    }
    
    AlirValue *dest = new_temp(ctx, (VarType){TYPE_INT, 0});
    call->dest = dest;
    emit(ctx, call);
    return dest;
}

AlirValue* alir_gen_expr(AlirCtx *ctx, ASTNode *node) {
    if (!node) return NULL;
    switch(node->type) {
        case NODE_LITERAL: return alir_gen_literal(ctx, (LiteralNode*)node);
        case NODE_VAR_REF: return alir_gen_var_ref(ctx, (VarRefNode*)node);
        case NODE_BINARY_OP: return alir_gen_binary_op(ctx, (BinaryOpNode*)node);
        case NODE_MEMBER_ACCESS: return alir_gen_access(ctx, node);
        case NODE_ARRAY_ACCESS: return alir_gen_access(ctx, node);
        case NODE_CALL: return alir_gen_call(ctx, (CallNode*)node);
        case NODE_METHOD_CALL: return alir_gen_method_call(ctx, (MethodCallNode*)node);
        // ... (Unary, Cast, etc)
        default: return NULL;
    }
}

void alir_gen_switch(AlirCtx *ctx, SwitchNode *sn) {
    AlirValue *cond = alir_gen_expr(ctx, sn->condition);
    AlirBlock *end_bb = alir_add_block(ctx->current_func, "switch_end");
    AlirBlock *default_bb = end_bb; 
    
    if (sn->default_case) default_bb = alir_add_block(ctx->current_func, "switch_default");

    // Create Switch Instruction
    AlirInst *sw = mk_inst(ALIR_OP_SWITCH, NULL, cond, alir_val_label(default_bb->label));
    sw->cases = NULL;
    AlirSwitchCase **tail = &sw->cases;

    // Pre-scan cases to build jump table
    ASTNode *c = sn->cases;
    while(c) {
        CaseNode *cn = (CaseNode*)c;
        AlirBlock *case_bb = alir_add_block(ctx->current_func, "case");
        
        // Add to instruction list
        AlirSwitchCase *sc = calloc(1, sizeof(AlirSwitchCase));
        sc->label = case_bb->label;
        if (cn->value->type == NODE_LITERAL) 
            sc->value = ((LiteralNode*)cn->value)->val.int_val;
        
        *tail = sc;
        tail = &sc->next;
        
        c = c->next;
    }
    emit(ctx, sw); // Emit switch at end of current block

    // Generate Bodies
    c = sn->cases;
    AlirSwitchCase *sc_iter = sw->cases;
    while(c) {
        CaseNode *cn = (CaseNode*)c;
        
        // Find the block we assigned
        AlirBlock *case_bb = NULL;
        AlirBlock *search = ctx->current_func->blocks;
        while(search) { 
            if (strcmp(search->label, sc_iter->label) == 0) { case_bb = search; break; }
            search = search->next;
        }
        
        ctx->current_block = case_bb;
        push_loop(ctx, NULL, end_bb); // Break inside switch goes to end
        
        ASTNode *stmt = cn->body;
        while(stmt) { alir_gen_stmt(ctx, stmt); stmt = stmt->next; }
        
        if (!cn->is_leak) emit(ctx, mk_inst(ALIR_OP_BR, NULL, alir_val_label(end_bb->label), NULL));
        
        pop_loop(ctx);
        
        c = c->next;
        sc_iter = sc_iter->next;
    }
    
    if (sn->default_case) {
        ctx->current_block = default_bb;
        push_loop(ctx, NULL, end_bb);
        ASTNode *stmt = sn->default_case;
        while(stmt) { alir_gen_stmt(ctx, stmt); stmt = stmt->next; }
        pop_loop(ctx);
        emit(ctx, mk_inst(ALIR_OP_BR, NULL, alir_val_label(end_bb->label), NULL));
    }
    
    ctx->current_block = end_bb;
}

void alir_gen_flux_yield(AlirCtx *ctx, EmitNode *en) {
    AlirValue *val = alir_gen_expr(ctx, en->value);
    emit(ctx, mk_inst(ALIR_OP_YIELD, NULL, val, NULL));
}

void alir_gen_stmt(AlirCtx *ctx, ASTNode *node) {
    if (!node) return;
    switch(node->type) {
        case NODE_VAR_DECL: {
            VarDeclNode *vn = (VarDeclNode*)node;
            AlirValue *ptr = new_temp(ctx, vn->var_type);
            emit(ctx, mk_inst(ALIR_OP_ALLOCA, ptr, NULL, NULL));
            alir_add_symbol(ctx, vn->name, ptr, vn->var_type);
            if (vn->initializer) {
                AlirValue *val = alir_gen_expr(ctx, vn->initializer);
                emit(ctx, mk_inst(ALIR_OP_STORE, NULL, val, ptr));
            }
            break;
        }
        case NODE_ASSIGN: {
            AssignNode *an = (AssignNode*)node;
            AlirValue *ptr = NULL;
            if (an->name) {
                AlirSymbol *s = alir_find_symbol(ctx, an->name);
                if (s) ptr = s->ptr;
            } else if (an->target) {
                ptr = alir_gen_addr(ctx, an->target);
            }
            AlirValue *val = alir_gen_expr(ctx, an->value);
            emit(ctx, mk_inst(ALIR_OP_STORE, NULL, val, ptr));
            break;
        }
        case NODE_SWITCH: alir_gen_switch(ctx, (SwitchNode*)node); break;
        case NODE_EMIT: alir_gen_flux_yield(ctx, (EmitNode*)node); break;
        case NODE_RETURN: {
            ReturnNode *rn = (ReturnNode*)node;
            AlirValue *v = rn->value ? alir_gen_expr(ctx, rn->value) : NULL;
            emit(ctx, mk_inst(ALIR_OP_RET, NULL, v, NULL));
            break;
        }
        case NODE_CALL: alir_gen_expr(ctx, node); break;
        case NODE_IF: {
            // Basic If Implementation (re-implementation for completeness)
            IfNode *in = (IfNode*)node;
            AlirValue *cond = alir_gen_expr(ctx, in->condition);
            AlirBlock *then_bb = alir_add_block(ctx->current_func, "then");
            AlirBlock *else_bb = alir_add_block(ctx->current_func, "else");
            AlirBlock *merge_bb = alir_add_block(ctx->current_func, "merge");
            
            AlirBlock *target_else = in->else_body ? else_bb : merge_bb;
            
            AlirInst *br = mk_inst(ALIR_OP_COND_BR, NULL, cond, alir_val_label(then_bb->label));
            br->args = malloc(sizeof(AlirValue*));
            br->args[0] = alir_val_label(target_else->label);
            br->arg_count = 1;
            emit(ctx, br);
            
            ctx->current_block = then_bb;
            ASTNode *s = in->then_body; while(s){ alir_gen_stmt(ctx,s); s=s->next; }
            emit(ctx, mk_inst(ALIR_OP_BR, NULL, alir_val_label(merge_bb->label), NULL));
            
            if (in->else_body) {
                ctx->current_block = else_bb;
                s = in->else_body; while(s){ alir_gen_stmt(ctx,s); s=s->next; }
                emit(ctx, mk_inst(ALIR_OP_BR, NULL, alir_val_label(merge_bb->label), NULL));
            }
            
            ctx->current_block = merge_bb;
            break;
        }
    }
}

AlirModule* alir_generate(ASTNode *root) {
    AlirCtx ctx;
    memset(&ctx, 0, sizeof(AlirCtx));
    ctx.module = alir_create_module("main_module");
    
    // 1. SCAN AND REGISTER CLASSES (The new smart pass)
    alir_scan_and_register_classes(&ctx, root);
    
    // 2. GEN FUNCTIONS
    ASTNode *curr = root;
    while(curr) {
        if (curr->type == NODE_FUNC_DEF) {
            FuncDefNode *fn = (FuncDefNode*)curr;
            ctx.current_func = alir_add_function(ctx.module, fn->name, fn->ret_type, fn->is_flux);
            
            // Register parameters
            Parameter *p = fn->params;
            while(p) {
                alir_func_add_param(ctx.current_func, p->name, p->type);
                p = p->next;
            }

            // If function is external (no body), do not create entry blocks
            if (!fn->body) {
                curr = curr->next;
                continue;
            }

            ctx.current_block = alir_add_block(ctx.current_func, "entry");
            ctx.temp_counter = 0;
            ctx.symbols = NULL; 
            
            // Setup Params allocation in ALIR logic
            // Note: alir_add_symbol is for ALIR tracking, not just IR emission
            p = fn->params;
            while(p) {
                AlirValue *ptr = new_temp(&ctx, p->type);
                emit(&ctx, mk_inst(ALIR_OP_ALLOCA, ptr, NULL, NULL));
                alir_add_symbol(&ctx, p->name, ptr, p->type);
                p = p->next;
            }
            
            ASTNode *stmt = fn->body;
            while(stmt) { alir_gen_stmt(&ctx, stmt); stmt = stmt->next; }
        } else if (curr->type == NODE_NAMESPACE) {
             // Recursive scan for functions inside namespaces would go here
             // simplified for flat list
        }
        curr = curr->next;
    }
    return ctx.module;
}
