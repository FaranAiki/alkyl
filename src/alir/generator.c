#include "alir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    // Basic Promotion Logic: Check base types
    if (v->type.base == target.base && v->type.ptr_depth == target.ptr_depth) return v;
    
    AlirValue *dest = new_temp(ctx, target);
    emit(ctx, mk_inst(ALIR_OP_CAST, dest, v, NULL));
    return dest;
}

// Symbol Table (IR Level: Maps names to Allocas/Registers)
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
    // Copy parent pointers
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

// --- FLUX HELPER FUNCTIONS ---

void collect_flux_vars_recursive(AlirCtx *ctx, ASTNode *node, int *idx_ptr) {
    if (!node) return;
    
    if (node->type == NODE_VAR_DECL) {
        VarDeclNode *vn = (VarDeclNode*)node;
        FluxVar *fv = calloc(1, sizeof(FluxVar));
        fv->name = strdup(vn->name);
        fv->type = vn->var_type;
        fv->index = (*idx_ptr)++;
        fv->next = ctx->flux_vars;
        ctx->flux_vars = fv;
    }
    else if (node->type == NODE_IF) {
        collect_flux_vars_recursive(ctx, ((IfNode*)node)->then_body, idx_ptr);
        collect_flux_vars_recursive(ctx, ((IfNode*)node)->else_body, idx_ptr);
    } 
    else if (node->type == NODE_WHILE) {
        collect_flux_vars_recursive(ctx, ((WhileNode*)node)->body, idx_ptr);
    }
    else if (node->type == NODE_LOOP) {
        collect_flux_vars_recursive(ctx, ((LoopNode*)node)->body, idx_ptr);
    }
    else if (node->type == NODE_FOR_IN) {
        ForInNode *fn = (ForInNode*)node;
        FluxVar *fv = calloc(1, sizeof(FluxVar));
        fv->name = strdup(fn->var_name);
        fv->type = fn->iter_type; // Usually AUTO, should be resolved
        if (fv->type.base == TYPE_AUTO) fv->type = (VarType){TYPE_INT}; // Fallback if not resolved
        fv->index = (*idx_ptr)++;
        fv->next = ctx->flux_vars;
        ctx->flux_vars = fv;
        collect_flux_vars_recursive(ctx, fn->body, idx_ptr);
    }
    else if (node->type == NODE_SWITCH) {
        ASTNode *c = ((SwitchNode*)node)->cases;
        while(c) {
            collect_flux_vars_recursive(ctx, ((CaseNode*)c)->body, idx_ptr);
            c = c->next;
        }
        collect_flux_vars_recursive(ctx, ((SwitchNode*)node)->default_case, idx_ptr);
    }
    
    collect_flux_vars_recursive(ctx, node->next, idx_ptr);
}

// --- LOWERING HELPER: Register Class Layout with Flattening ---
void alir_scan_and_register_classes(AlirCtx *ctx, ASTNode *root) {
    ASTNode *curr = root;
    while(curr) {
        if (curr->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)curr;
            
            AlirField *head = NULL;
            AlirField **tail = &head;
            int idx = 0;

            // 1. FLATTENING: Copy Parent Fields First
            if (cn->parent_name) {
                AlirStruct *parent = alir_find_struct(ctx->module, cn->parent_name);
                if (parent) {
                    AlirField *pf = parent->fields;
                    while(pf) {
                        AlirField *nf = calloc(1, sizeof(AlirField));
                        nf->name = strdup(pf->name); // Copy name
                        nf->type = pf->type;
                        nf->index = idx++;
                        
                        *tail = nf;
                        tail = &nf->next;
                        pf = pf->next;
                    }
                }
            }

            // 2. Add Local Members
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
             alir_scan_and_register_classes(ctx, ((NamespaceNode*)curr)->body);
        }
        curr = curr->next;
    }
}

// --- LOWERING CONSTRUCTOR ---
AlirValue* alir_lower_new_object(AlirCtx *ctx, const char *class_name, ASTNode *args) {
    // Verify struct exists in IR
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

    // 4. Call Constructor
    // Note: In a real compiler, we'd mangle the constructor name properly or look it up via SemCtx
    AlirInst *call_init = mk_inst(ALIR_OP_CALL, NULL, alir_val_var(class_name), NULL);
    
    int arg_count = 0; ASTNode *a = args; while(a) { arg_count++; a=a->next; }
    call_init->arg_count = arg_count + 1;
    call_init->args = malloc(sizeof(AlirValue*) * (arg_count + 1));
    
    call_init->args[0] = obj_ptr; // THIS pointer
    
    int i = 1; a = args;
    while(a) {
        call_init->args[i++] = alir_gen_expr(ctx, a);
        a = a->next;
    }
    
    call_init->dest = new_temp(ctx, (VarType){TYPE_VOID, 0});
    emit(ctx, call_init);
    
    return obj_ptr;
}


// --- L-VALUE GENERATION ---

AlirValue* alir_gen_addr(AlirCtx *ctx, ASTNode *node) {
    if (node->type == NODE_VAR_REF) {
        VarRefNode *vn = (VarRefNode*)node;
        // Check IR local symbols first
        AlirSymbol *sym = alir_find_symbol(ctx, vn->name);
        if (sym) return sym->ptr;
        // If not found locally, assume global
        return alir_val_var(vn->name);
    }
    
    if (node->type == NODE_MEMBER_ACCESS) {
        MemberAccessNode *ma = (MemberAccessNode*)node;
        AlirValue *base_ptr = alir_gen_addr(ctx, ma->object);
        if (!base_ptr) base_ptr = alir_gen_expr(ctx, ma->object);

        // Retrieve accurate type from Semantic Context
        VarType obj_t = sem_get_node_type(ctx->sem, ma->object);
        
        if (obj_t.class_name) {
            int idx = alir_get_field_index(ctx->module, obj_t.class_name, ma->member_name);
            if (idx != -1) {
                AlirValue *res = new_temp(ctx, (VarType){TYPE_INT, 1}); // Pointer to int? Should be pointer to field type
                // In full implementation, lookup field type from struct registry
                emit(ctx, mk_inst(ALIR_OP_GET_PTR, res, base_ptr, alir_const_int(idx)));
                return res;
            }
        }
    }
    
    if (node->type == NODE_ARRAY_ACCESS) {
        ArrayAccessNode *aa = (ArrayAccessNode*)node;
        AlirValue *base_ptr = alir_gen_addr(ctx, aa->target);
        AlirValue *index = alir_gen_expr(ctx, aa->index);
        
        // Result is pointer to element
        // We really should know the element type here.
        VarType elem_t = sem_get_node_type(ctx->sem, (ASTNode*)aa);
        elem_t.ptr_depth++; // Make it a pointer (L-Value)

        AlirValue *res = new_temp(ctx, elem_t);
        emit(ctx, mk_inst(ALIR_OP_GET_PTR, res, base_ptr, index));
        return res;
    }
    
    return NULL;
}

// --- TRAIT ACCESS GEN ---
AlirValue* alir_gen_trait_access(AlirCtx *ctx, TraitAccessNode *ta) {
    AlirValue *base_ptr = alir_gen_addr(ctx, ta->object);
    if (!base_ptr) base_ptr = alir_gen_expr(ctx, ta->object);
    
    VarType obj_t = sem_get_node_type(ctx->sem, ta->object);
    
    // 1. Try to find a field named after the Trait (Mixin strategy)
    if (obj_t.class_name) {
        int idx = alir_get_field_index(ctx->module, obj_t.class_name, ta->trait_name);
        if (idx != -1) {
            // Found explicit field for trait
            AlirValue *res = new_temp(ctx, (VarType){TYPE_CLASS, 1, strdup(ta->trait_name)});
            emit(ctx, mk_inst(ALIR_OP_GET_PTR, res, base_ptr, alir_const_int(idx)));
            return res;
        }
    }
    
    // 2. Fallback: Bitcast (Unsafe/Direct Cast)
    VarType trait_ptr_t = {TYPE_CLASS, 1, strdup(ta->trait_name)};
    AlirValue *cast_res = new_temp(ctx, trait_ptr_t);
    emit(ctx, mk_inst(ALIR_OP_BITCAST, cast_res, base_ptr, NULL));
    return cast_res;
}

AlirValue* alir_gen_literal(AlirCtx *ctx, LiteralNode *ln) {
    if (ln->var_type.base == TYPE_INT) return alir_const_int(ln->val.int_val);
    if (ln->var_type.base == TYPE_FLOAT) return alir_const_float(ln->val.double_val);
    if (ln->var_type.base == TYPE_STRING) {
        return alir_module_add_string_literal(ctx->module, ln->val.str_val, ctx->str_counter++);
    }
    // Fallback
    return alir_const_int(0);
}

AlirValue* alir_gen_var_ref(AlirCtx *ctx, VarRefNode *vn) {
    AlirValue *ptr = alir_gen_addr(ctx, (ASTNode*)vn);
    
    // Get precise type from Semantics
    VarType t = sem_get_node_type(ctx->sem, (ASTNode*)vn);
    
    AlirValue *val = new_temp(ctx, t);
    emit(ctx, mk_inst(ALIR_OP_LOAD, val, ptr, NULL));
    return val;
}

AlirValue* alir_gen_access(AlirCtx *ctx, ASTNode *node) {
    AlirValue *ptr = alir_gen_addr(ctx, node);
    
    VarType t = sem_get_node_type(ctx->sem, node);
    
    AlirValue *val = new_temp(ctx, t); 
    emit(ctx, mk_inst(ALIR_OP_LOAD, val, ptr, NULL));
    return val;
}

AlirValue* alir_gen_binary_op(AlirCtx *ctx, BinaryOpNode *bn) {
    AlirValue *l = alir_gen_expr(ctx, bn->left);
    AlirValue *r = alir_gen_expr(ctx, bn->right);
    
    // Check types via Semantic Context to decide on Float vs Int ops
    VarType l_type = sem_get_node_type(ctx->sem, bn->left);
    VarType r_type = sem_get_node_type(ctx->sem, bn->right);

    int is_float = (l_type.base == TYPE_FLOAT || l_type.base == TYPE_DOUBLE ||
                    r_type.base == TYPE_FLOAT || r_type.base == TYPE_DOUBLE);

    if (is_float) {
        VarType target = {TYPE_DOUBLE, 0}; // Default to double for mixed
        l = promote(ctx, l, target);
        r = promote(ctx, r, target);
    }

    AlirOpcode op = ALIR_OP_ADD;
    switch(bn->op) {
        case TOKEN_PLUS: op = is_float ? ALIR_OP_FADD : ALIR_OP_ADD; break;
        case TOKEN_MINUS: op = is_float ? ALIR_OP_FSUB : ALIR_OP_SUB; break;
        case TOKEN_STAR: op = is_float ? ALIR_OP_FMUL : ALIR_OP_MUL; break;
        case TOKEN_SLASH: op = is_float ? ALIR_OP_FDIV : ALIR_OP_DIV; break;
        case TOKEN_EQ: op = ALIR_OP_EQ; break;
        case TOKEN_LT: op = ALIR_OP_LT; break;
        // ... add other cases
    }
    
    // Result type logic
    VarType res_type = is_float ? (VarType){TYPE_DOUBLE, 0} : (VarType){TYPE_INT, 0};
    if (op == ALIR_OP_EQ || op == ALIR_OP_LT) res_type = (VarType){TYPE_BOOL, 0};
    
    AlirValue *dest = new_temp(ctx, res_type);
    emit(ctx, mk_inst(op, dest, l, r));
    return dest;
}

AlirValue* alir_gen_call_std(AlirCtx *ctx, CallNode *cn) {
    AlirInst *call = mk_inst(ALIR_OP_CALL, NULL, alir_val_var(cn->name), NULL);
    
    int count = 0; ASTNode *a = cn->args; while(a) { count++; a=a->next; }
    call->arg_count = count;
    call->args = malloc(sizeof(AlirValue*) * count);
    
    int i = 0; a = cn->args;
    while(a) {
        call->args[i++] = alir_gen_expr(ctx, a);
        a = a->next;
    }
    
    // Result type from Semantic Table
    VarType ret_type = sem_get_node_type(ctx->sem, (ASTNode*)cn);
    
    AlirValue *dest = new_temp(ctx, ret_type); 
    call->dest = dest;
    emit(ctx, call);
    return dest;
}

AlirValue* alir_gen_call(AlirCtx *ctx, CallNode *cn) {
    // Check if it's a constructor call via Struct Registry
    if (alir_find_struct(ctx->module, cn->name)) {
        return alir_lower_new_object(ctx, cn->name, cn->args);
    }
    return alir_gen_call_std(ctx, cn);
}

AlirValue* alir_gen_method_call(AlirCtx *ctx, MethodCallNode *mc) {
    AlirValue *this_ptr = alir_gen_addr(ctx, mc->object);
    if (!this_ptr) this_ptr = alir_gen_expr(ctx, mc->object); 

    // Mangle: Class_Method
    VarType obj_t = sem_get_node_type(ctx->sem, mc->object);
    char func_name[256];
    if (obj_t.class_name) snprintf(func_name, 256, "%s_%s", obj_t.class_name, mc->method_name);
    else snprintf(func_name, 256, "%s", mc->method_name);

    AlirInst *call = mk_inst(ALIR_OP_CALL, NULL, alir_val_var(func_name), NULL);
    
    int count = 0; ASTNode *a = mc->args; while(a) { count++; a=a->next; }
    call->arg_count = count + 1;
    call->args = malloc(sizeof(AlirValue*) * (count + 1));
    
    call->args[0] = this_ptr;
    int i = 1; a = mc->args;
    while(a) {
        call->args[i++] = alir_gen_expr(ctx, a);
        a = a->next;
    }
    
    VarType ret_type = sem_get_node_type(ctx->sem, (ASTNode*)mc);
    AlirValue *dest = new_temp(ctx, ret_type);
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
        case NODE_TRAIT_ACCESS: return alir_gen_trait_access(ctx, (TraitAccessNode*)node);
        default: return NULL;
    }
}

void alir_gen_switch(AlirCtx *ctx, SwitchNode *sn) {
    AlirValue *cond = alir_gen_expr(ctx, sn->condition);
    AlirBlock *end_bb = alir_add_block(ctx->current_func, "switch_end");
    AlirBlock *default_bb = end_bb; 
    
    if (sn->default_case) default_bb = alir_add_block(ctx->current_func, "switch_default");

    AlirInst *sw = mk_inst(ALIR_OP_SWITCH, NULL, cond, alir_val_label(default_bb->label));
    sw->cases = NULL;
    AlirSwitchCase **tail = &sw->cases;

    ASTNode *c = sn->cases;
    while(c) {
        CaseNode *cn = (CaseNode*)c;
        AlirBlock *case_bb = alir_add_block(ctx->current_func, "case");
        
        AlirSwitchCase *sc = calloc(1, sizeof(AlirSwitchCase));
        sc->label = case_bb->label;
        if (cn->value->type == NODE_LITERAL) 
            sc->value = ((LiteralNode*)cn->value)->val.int_val;
        
        *tail = sc;
        tail = &sc->next;
        
        c = c->next;
    }
    emit(ctx, sw); 

    c = sn->cases;
    AlirSwitchCase *sc_iter = sw->cases;
    while(c) {
        CaseNode *cn = (CaseNode*)c;
        AlirBlock *case_bb = NULL;
        AlirBlock *search = ctx->current_func->blocks;
        while(search) { 
            if (strcmp(search->label, sc_iter->label) == 0) { case_bb = search; break; }
            search = search->next;
        }
        
        ctx->current_block = case_bb;
        push_loop(ctx, NULL, end_bb);
        
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
    if (ctx->in_flux_resume) {
        // --- FLUX YIELD LOWERING ---
        // 1. Evaluate Value
        AlirValue *val = alir_gen_expr(ctx, en->value);
        
        // 2. Store to Context->Result (Index 2)
        // struct { state, finished, result, ... }
        AlirValue *res_ptr = new_temp(ctx, val->type); // Should correspond to yield type
        emit(ctx, mk_inst(ALIR_OP_GET_PTR, res_ptr, ctx->flux_ctx_ptr, alir_const_int(2)));
        emit(ctx, mk_inst(ALIR_OP_STORE, NULL, val, res_ptr));
        
        // 3. Update State
        int next_state = ctx->flux_yield_count++;
        AlirValue *state_ptr = new_temp(ctx, (VarType){TYPE_INT, 1});
        emit(ctx, mk_inst(ALIR_OP_GET_PTR, state_ptr, ctx->flux_ctx_ptr, alir_const_int(0)));
        emit(ctx, mk_inst(ALIR_OP_STORE, NULL, alir_const_int(next_state), state_ptr));
        
        // 4. Return Void
        emit(ctx, mk_inst(ALIR_OP_RET, NULL, NULL, NULL));
        
        // 5. Create Resume Block for Next State
        char label[32]; sprintf(label, "resume_%d", next_state);
        AlirBlock *resume_bb = alir_add_block(ctx->current_func, label);
        ctx->current_block = resume_bb;
        
        // 6. Patch Switch
        AlirSwitchCase *nc = calloc(1, sizeof(AlirSwitchCase));
        nc->value = next_state;
        nc->label = resume_bb->label;
        nc->next = ctx->flux_resume_switch->cases;
        ctx->flux_resume_switch->cases = nc;
        
    } else {
        // Fallback for non-lowered yield (if supported directly)
        AlirValue *val = alir_gen_expr(ctx, en->value);
        emit(ctx, mk_inst(ALIR_OP_YIELD, NULL, val, NULL));
    }
}

void alir_gen_stmt(AlirCtx *ctx, ASTNode *node) {
    if (!node) return;

    if (node->type == NODE_VAR_DECL && ctx->in_flux_resume) {
        // --- FLUX VARIABLE DECLARATION ---
        VarDeclNode *vn = (VarDeclNode*)node;
        
        // Find pre-assigned index in flux context
        FluxVar *fv = ctx->flux_vars;
        while(fv) {
            if (strcmp(fv->name, vn->name) == 0) break;
            fv = fv->next;
        }
        
        if (fv) {
            // Get pointer to field in context
            VarType ptr_type = vn->var_type;
            ptr_type.ptr_depth++;
            AlirValue *ptr = new_temp(ctx, ptr_type);
            emit(ctx, mk_inst(ALIR_OP_GET_PTR, ptr, ctx->flux_ctx_ptr, alir_const_int(fv->index)));
            
            // Register symbol as pointing to this field
            alir_add_symbol(ctx, vn->name, ptr, vn->var_type);
            
            if (vn->initializer) {
                AlirValue *val = alir_gen_expr(ctx, vn->initializer);
                emit(ctx, mk_inst(ALIR_OP_STORE, NULL, val, ptr));
            }
            return; // Skip standard ALLOCA logic
        }
        // Fallthrough if not found (shouldn't happen if collector works)
    }

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
                // Find IR register holding the variable address
                AlirSymbol *s = alir_find_symbol(ctx, an->name);
                if (s) ptr = s->ptr;
                else ptr = alir_val_var(an->name); // Global fallback
            } else if (an->target) {
                ptr = alir_gen_addr(ctx, an->target);
            }
            AlirValue *val = alir_gen_expr(ctx, an->value);
            emit(ctx, mk_inst(ALIR_OP_STORE, NULL, val, ptr));
            break;
        }
        case NODE_SWITCH: alir_gen_switch(ctx, (SwitchNode*)node); break;
        case NODE_EMIT: alir_gen_flux_yield(ctx, (EmitNode*)node); break;
        
        case NODE_WHILE: {
            WhileNode *wn = (WhileNode*)node;
            AlirBlock *cond_bb = alir_add_block(ctx->current_func, "while_cond");
            AlirBlock *body_bb = alir_add_block(ctx->current_func, "while_body");
            AlirBlock *end_bb = alir_add_block(ctx->current_func, "while_end");

            if (wn->is_do_while) {
                // Do-While: Body -> Cond -> Body/End
                // Initial jump to body
                emit(ctx, mk_inst(ALIR_OP_BR, NULL, alir_val_label(body_bb->label), NULL));
                
                // Body
                ctx->current_block = body_bb;
                push_loop(ctx, cond_bb, end_bb);
                ASTNode *s = wn->body; while(s) { alir_gen_stmt(ctx, s); s=s->next; }
                pop_loop(ctx);
                
                // Fallthrough to Cond
                emit(ctx, mk_inst(ALIR_OP_BR, NULL, alir_val_label(cond_bb->label), NULL));

                // Cond
                ctx->current_block = cond_bb;
                AlirValue *cond = alir_gen_expr(ctx, wn->condition);
                AlirInst *br = mk_inst(ALIR_OP_COND_BR, NULL, cond, alir_val_label(body_bb->label));
                br->args = malloc(sizeof(AlirValue*));
                br->args[0] = alir_val_label(end_bb->label);
                br->arg_count = 1;
                emit(ctx, br);
            } else {
                // While: Cond -> Body -> Cond / End
                emit(ctx, mk_inst(ALIR_OP_BR, NULL, alir_val_label(cond_bb->label), NULL));

                // Cond
                ctx->current_block = cond_bb;
                AlirValue *cond = alir_gen_expr(ctx, wn->condition);
                AlirInst *br = mk_inst(ALIR_OP_COND_BR, NULL, cond, alir_val_label(body_bb->label));
                br->args = malloc(sizeof(AlirValue*));
                br->args[0] = alir_val_label(end_bb->label);
                br->arg_count = 1;
                emit(ctx, br);

                // Body
                ctx->current_block = body_bb;
                push_loop(ctx, cond_bb, end_bb);
                ASTNode *s = wn->body; while(s) { alir_gen_stmt(ctx, s); s=s->next; }
                pop_loop(ctx);
                
                // Jump back to Cond
                emit(ctx, mk_inst(ALIR_OP_BR, NULL, alir_val_label(cond_bb->label), NULL));
            }
            ctx->current_block = end_bb;
            break;
        }

        case NODE_LOOP: {
            LoopNode *ln = (LoopNode*)node;
            AlirBlock *body_bb = alir_add_block(ctx->current_func, "loop_body");
            AlirBlock *end_bb = alir_add_block(ctx->current_func, "loop_end");
            
            emit(ctx, mk_inst(ALIR_OP_BR, NULL, alir_val_label(body_bb->label), NULL));
            
            ctx->current_block = body_bb;
            push_loop(ctx, body_bb, end_bb); // Continue goes to start of body
            
            ASTNode *s = ln->body; while(s) { alir_gen_stmt(ctx, s); s=s->next; }
            pop_loop(ctx);
            emit(ctx, mk_inst(ALIR_OP_BR, NULL, alir_val_label(body_bb->label), NULL));
            
            ctx->current_block = end_bb;
            break;
        }

        case NODE_FOR_IN: {
            ForInNode *fn = (ForInNode*)node;
            AlirValue *col = alir_gen_expr(ctx, fn->collection);
            
            // Create Opaque Iterator
            AlirValue *iter = new_temp(ctx, (VarType){TYPE_VOID, 1}); 
            emit(ctx, mk_inst(ALIR_OP_ITER_INIT, iter, col, NULL));
            
            AlirBlock *cond_bb = alir_add_block(ctx->current_func, "for_cond");
            AlirBlock *body_bb = alir_add_block(ctx->current_func, "for_body");
            AlirBlock *end_bb = alir_add_block(ctx->current_func, "for_end");
            
            emit(ctx, mk_inst(ALIR_OP_BR, NULL, alir_val_label(cond_bb->label), NULL));
            
            // Condition: ITER_VALID
            ctx->current_block = cond_bb;
            AlirValue *valid = new_temp(ctx, (VarType){TYPE_BOOL, 0});
            emit(ctx, mk_inst(ALIR_OP_ITER_VALID, valid, iter, NULL));
            
            AlirInst *br = mk_inst(ALIR_OP_COND_BR, NULL, valid, alir_val_label(body_bb->label));
            br->args = malloc(sizeof(AlirValue*));
            br->args[0] = alir_val_label(end_bb->label);
            br->arg_count = 1;
            emit(ctx, br);
            
            // Body
            ctx->current_block = body_bb;
            push_loop(ctx, cond_bb, end_bb); // Continue checks condition again (and next called after body)
            
            // Extract Value: ITER_GET
            AlirValue *val = new_temp(ctx, (VarType){TYPE_AUTO}); // Type resolved at runtime/linktime or via semctx
            emit(ctx, mk_inst(ALIR_OP_ITER_GET, val, iter, NULL));
            
            // Store to local loop variable (Handled by special logic if in flux)
            if (ctx->in_flux_resume) {
                // Find pre-assigned field
                FluxVar *fv = ctx->flux_vars;
                while(fv) { if(strcmp(fv->name, fn->var_name)==0) break; fv=fv->next; }
                if (fv) {
                    AlirValue *ptr = new_temp(ctx, (VarType){TYPE_INT, 1}); // Simplified type
                    emit(ctx, mk_inst(ALIR_OP_GET_PTR, ptr, ctx->flux_ctx_ptr, alir_const_int(fv->index)));
                    alir_add_symbol(ctx, fn->var_name, ptr, fn->iter_type);
                    emit(ctx, mk_inst(ALIR_OP_STORE, NULL, val, ptr));
                }
            } else {
                AlirValue *var_ptr = new_temp(ctx, (VarType){TYPE_AUTO}); 
                emit(ctx, mk_inst(ALIR_OP_ALLOCA, var_ptr, NULL, NULL));
                alir_add_symbol(ctx, fn->var_name, var_ptr, (VarType){TYPE_AUTO});
                emit(ctx, mk_inst(ALIR_OP_STORE, NULL, val, var_ptr));
            }
            
            ASTNode *s = fn->body; while(s) { alir_gen_stmt(ctx, s); s=s->next; }
            
            // Step: ITER_NEXT
            emit(ctx, mk_inst(ALIR_OP_ITER_NEXT, NULL, iter, NULL));
            emit(ctx, mk_inst(ALIR_OP_BR, NULL, alir_val_label(cond_bb->label), NULL));
            
            pop_loop(ctx);
            ctx->current_block = end_bb;
            break;
        }

        case NODE_BREAK:
            if (ctx->loop_break) emit(ctx, mk_inst(ALIR_OP_BR, NULL, alir_val_label(ctx->loop_break->label), NULL));
            break;
            
        case NODE_CONTINUE:
            if (ctx->loop_continue) emit(ctx, mk_inst(ALIR_OP_BR, NULL, alir_val_label(ctx->loop_continue->label), NULL));
            break;

        case NODE_RETURN: {
            ReturnNode *rn = (ReturnNode*)node;
            if (ctx->in_flux_resume) {
                // Terminate Flux
                AlirValue *fin_ptr = new_temp(ctx, (VarType){TYPE_BOOL, 1});
                emit(ctx, mk_inst(ALIR_OP_GET_PTR, fin_ptr, ctx->flux_ctx_ptr, alir_const_int(1))); // finished at idx 1
                emit(ctx, mk_inst(ALIR_OP_STORE, NULL, alir_const_int(1), fin_ptr));
                emit(ctx, mk_inst(ALIR_OP_RET, NULL, NULL, NULL)); // Return void from resume
            } else {
                AlirValue *v = rn->value ? alir_gen_expr(ctx, rn->value) : NULL;
                emit(ctx, mk_inst(ALIR_OP_RET, NULL, v, NULL));
            }
            break;
        }
        case NODE_CALL: alir_gen_expr(ctx, node); break;
        case NODE_IF: {
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

// --- FLUX FUNCTION GENERATION ---
void alir_gen_flux_def(AlirCtx *ctx, FuncDefNode *fn) {
    // 1. Collect Variables to Capture
    ctx->flux_vars = NULL;
    int struct_idx = 3; // 0=state, 1=finished, 2=result
    
    // Count Params
    int param_count = 0;
    Parameter *p = fn->params;
    while(p) { param_count++; p = p->next; }
    
    // Total struct start index for locals = 3 + param_count + (1 if class member)
    int start_locals = struct_idx + param_count + (fn->class_name ? 1 : 0);
    int current_idx = start_locals;
    
    collect_flux_vars_recursive(ctx, fn->body, &current_idx);
    
    // 2. Register Context Struct
    char struct_name[256];
    snprintf(struct_name, 256, "FluxCtx_%s", fn->name);
    
    AlirField *fields = NULL;
    AlirField **tail = &fields;
    
    // Add Header Fields
    // Field 0: state (int)
    { AlirField *f = calloc(1, sizeof(AlirField)); f->name = strdup("state"); f->type = (VarType){TYPE_INT}; f->index=0; *tail=f; tail=&f->next; }
    // Field 1: finished (bool)
    { AlirField *f = calloc(1, sizeof(AlirField)); f->name = strdup("finished"); f->type = (VarType){TYPE_BOOL}; f->index=1; *tail=f; tail=&f->next; }
    // Field 2: result (ret_type)
    { AlirField *f = calloc(1, sizeof(AlirField)); f->name = strdup("result"); f->type = fn->ret_type; f->index=2; *tail=f; tail=&f->next; }
    
    // Add Params
    int p_idx = 3;
    if (fn->class_name) {
        AlirField *f = calloc(1, sizeof(AlirField));
        f->name = strdup("this");
        f->type = (VarType){TYPE_CLASS, 1, strdup(fn->class_name)}; // Pointer to class
        f->index = p_idx++;
        *tail=f; tail=&f->next;
    }
    p = fn->params;
    while(p) {
        AlirField *f = calloc(1, sizeof(AlirField));
        f->name = strdup(p->name);
        f->type = p->type;
        f->index = p_idx++;
        *tail=f; tail=&f->next;
        p = p->next;
    }
    
    // Add Locals
    FluxVar *fv = ctx->flux_vars;
    while(fv) {
        AlirField *f = calloc(1, sizeof(AlirField));
        f->name = strdup(fv->name);
        f->type = fv->type;
        f->index = fv->index;
        *tail=f; tail=&f->next;
        fv = fv->next;
    }
    
    alir_register_struct(ctx->module, struct_name, fields);
    
    // 3. Generate INIT Function (The Generator Factory)
    ctx->current_func = alir_add_function(ctx->module, fn->name, (VarType){TYPE_CHAR, 1}, 0); // Returns char* (opaque ptr)
    // Add params to Init func
    if (fn->class_name) alir_func_add_param(ctx->current_func, "this", (VarType){TYPE_CLASS, 1, strdup(fn->class_name)});
    p = fn->params;
    while(p) {
        alir_func_add_param(ctx->current_func, p->name, p->type);
        p = p->next;
    }
    
    ctx->current_block = alir_add_block(ctx->current_func, "entry");
    
    // Allocate Struct
    AlirValue *size_val = new_temp(ctx, (VarType){TYPE_INT});
    emit(ctx, mk_inst(ALIR_OP_SIZEOF, size_val, alir_val_type(struct_name), NULL));
    
    AlirValue *raw_mem = new_temp(ctx, (VarType){TYPE_CHAR, 1});
    emit(ctx, mk_inst(ALIR_OP_ALLOC_HEAP, raw_mem, size_val, NULL));
    
    AlirValue *ctx_ptr = new_temp(ctx, (VarType){TYPE_CLASS, 1, strdup(struct_name)});
    emit(ctx, mk_inst(ALIR_OP_BITCAST, ctx_ptr, raw_mem, NULL));
    
    // Init Header: state=0, finished=0
    AlirValue *ptr_state = new_temp(ctx, (VarType){TYPE_INT, 1});
    emit(ctx, mk_inst(ALIR_OP_GET_PTR, ptr_state, ctx_ptr, alir_const_int(0)));
    emit(ctx, mk_inst(ALIR_OP_STORE, NULL, alir_const_int(0), ptr_state));
    
    AlirValue *ptr_fin = new_temp(ctx, (VarType){TYPE_BOOL, 1});
    emit(ctx, mk_inst(ALIR_OP_GET_PTR, ptr_fin, ctx_ptr, alir_const_int(1)));
    emit(ctx, mk_inst(ALIR_OP_STORE, NULL, alir_const_int(0), ptr_fin));
    
    // Store Params into Struct
    int param_offset = 0;
    p_idx = 3;
    if (fn->class_name) {
        AlirValue *this_arg = alir_val_temp((VarType){TYPE_CLASS, 1, strdup(fn->class_name)}, param_offset++); // Pseudo arg ref
        // Note: Real arg ref logic would use dedicated arg op/val, here relying on implicit register numbering for args 0..N
        // For simplicity, assuming args are mapped to %p0, %p1...
        
        // Actually, ALIR uses named parameters in function def.
        // We need to resolve param name to a value. 
        // ALIR doesn't have a direct "get param N" op in current definition, 
        // usually params are allocas. Here we just use ALIR_VAL_VAR or TEMP.
        // The printer uses %p0, %p1. 
        // Let's assume %p0 ...
        char arg_name[16]; sprintf(arg_name, "p%d", param_offset-1);
        AlirValue *arg_val = alir_val_var(arg_name); // Placeholder for arg value
        
        AlirValue *f_ptr = new_temp(ctx, (VarType){TYPE_CLASS, 2, strdup(fn->class_name)});
        emit(ctx, mk_inst(ALIR_OP_GET_PTR, f_ptr, ctx_ptr, alir_const_int(p_idx++)));
        emit(ctx, mk_inst(ALIR_OP_STORE, NULL, arg_val, f_ptr));
    }
    p = fn->params;
    while(p) {
        char arg_name[16]; sprintf(arg_name, "p%d", param_offset++);
        AlirValue *arg_val = alir_val_var(arg_name);
        
        VarType pt = p->type; pt.ptr_depth++;
        AlirValue *f_ptr = new_temp(ctx, pt);
        emit(ctx, mk_inst(ALIR_OP_GET_PTR, f_ptr, ctx_ptr, alir_const_int(p_idx++)));
        emit(ctx, mk_inst(ALIR_OP_STORE, NULL, arg_val, f_ptr));
        p = p->next;
    }
    
    // Return Context Ptr
    emit(ctx, mk_inst(ALIR_OP_RET, NULL, raw_mem, NULL));
    
    // 4. Generate RESUME Function
    char resume_name[256]; snprintf(resume_name, 256, "%s_Resume", fn->name);
    ctx->current_func = alir_add_function(ctx->module, resume_name, (VarType){TYPE_VOID}, 0);
    alir_func_add_param(ctx->current_func, "ctx", (VarType){TYPE_CHAR, 1}); // void* ctx
    
    ctx->current_block = alir_add_block(ctx->current_func, "entry");
    
    // Prepare Flux Context
    ctx->in_flux_resume = 1;
    ctx->flux_struct_name = strdup(struct_name);
    ctx->flux_yield_count = 1; // State 0 is entry, next is 1
    
    // Bitcast void* ctx to FluxCtx*
    AlirValue *void_ctx = alir_val_var("p0"); // First arg
    ctx->flux_ctx_ptr = new_temp(ctx, (VarType){TYPE_CLASS, 1, strdup(struct_name)});
    emit(ctx, mk_inst(ALIR_OP_BITCAST, ctx->flux_ctx_ptr, void_ctx, NULL));
    
    // Load State
    AlirValue *ptr_st = new_temp(ctx, (VarType){TYPE_INT, 1});
    emit(ctx, mk_inst(ALIR_OP_GET_PTR, ptr_st, ctx->flux_ctx_ptr, alir_const_int(0)));
    AlirValue *current_state = new_temp(ctx, (VarType){TYPE_INT});
    emit(ctx, mk_inst(ALIR_OP_LOAD, current_state, ptr_st, NULL));
    
    // Create Switch
    AlirBlock *start_bb = alir_add_block(ctx->current_func, "flux_start");
    AlirBlock *end_bb = alir_add_block(ctx->current_func, "flux_end");
    
    AlirInst *sw = mk_inst(ALIR_OP_SWITCH, NULL, current_state, alir_val_label(end_bb->label));
    ctx->flux_resume_switch = sw;
    
    // Case 0 -> Start
    AlirSwitchCase *c0 = calloc(1, sizeof(AlirSwitchCase));
    c0->value = 0; c0->label = start_bb->label;
    sw->cases = c0;
    emit(ctx, sw);
    
    // Populate Symbols for Parameters in Resume
    ctx->current_block = start_bb;
    ctx->symbols = NULL; // Clear symbols from Init func
    
    p_idx = 3;
    if (fn->class_name) {
         VarType pt = {TYPE_CLASS, 1, strdup(fn->class_name)}; pt.ptr_depth++; // Pointer to pointer
         AlirValue *ptr = new_temp(ctx, pt);
         emit(ctx, mk_inst(ALIR_OP_GET_PTR, ptr, ctx->flux_ctx_ptr, alir_const_int(p_idx++)));
         // Deref once to get the 'this' pointer value? 
         // Logic in alir_gen_var_ref does LOAD. So we need the address of the variable.
         // 'this' is stored in the struct. 'ptr' is the address of 'this' in the struct.
         // So adding 'ptr' to symbol table is correct.
         alir_add_symbol(ctx, "this", ptr, (VarType){TYPE_CLASS, 1, strdup(fn->class_name)});
    }
    p = fn->params;
    while(p) {
        VarType pt = p->type; pt.ptr_depth++;
        AlirValue *ptr = new_temp(ctx, pt);
        emit(ctx, mk_inst(ALIR_OP_GET_PTR, ptr, ctx->flux_ctx_ptr, alir_const_int(p_idx++)));
        alir_add_symbol(ctx, p->name, ptr, p->type);
        p = p->next;
    }
    
    // Generate Body
    ASTNode *stmt = fn->body;
    while(stmt) { alir_gen_stmt(ctx, stmt); stmt = stmt->next; }
    
    // Default Finish (if fallthrough)
    if (!ctx->current_block->tail || ctx->current_block->tail->op != ALIR_OP_RET) {
        // Set finished=1
        AlirValue *p_fin = new_temp(ctx, (VarType){TYPE_BOOL, 1});
        emit(ctx, mk_inst(ALIR_OP_GET_PTR, p_fin, ctx->flux_ctx_ptr, alir_const_int(1)));
        emit(ctx, mk_inst(ALIR_OP_STORE, NULL, alir_const_int(1), p_fin));
        emit(ctx, mk_inst(ALIR_OP_RET, NULL, NULL, NULL));
    }
    
    ctx->current_block = end_bb;
    emit(ctx, mk_inst(ALIR_OP_RET, NULL, NULL, NULL));
    
    // Cleanup
    ctx->in_flux_resume = 0;
    ctx->flux_vars = NULL;
    ctx->flux_resume_switch = NULL;
}

// MAIN ENTRY POINT
AlirModule* alir_generate(SemanticCtx *sem, ASTNode *root) {
    AlirCtx ctx;
    memset(&ctx, 0, sizeof(AlirCtx));
    ctx.sem = sem; // Store the Semantic Context
    ctx.module = alir_create_module("main_module");
    
    // 1. SCAN AND REGISTER CLASSES (Flattening included)
    alir_scan_and_register_classes(&ctx, root);
    
    // 2. GEN FUNCTIONS
    ASTNode *curr = root;
    while(curr) {
        if (curr->type == NODE_FUNC_DEF) {
            FuncDefNode *fn = (FuncDefNode*)curr;
            
            if (fn->is_flux) {
                // Specialized Flux Generation
                alir_gen_flux_def(&ctx, fn);
            } else {
                // Standard Function Generation
                ctx.current_func = alir_add_function(ctx.module, fn->name, fn->ret_type, 0);
                
                // Register parameters
                Parameter *p = fn->params;
                while(p) {
                    alir_func_add_param(ctx.current_func, p->name, p->type);
                    p = p->next;
                }

                if (!fn->body) { curr = curr->next; continue; }

                ctx.current_block = alir_add_block(ctx.current_func, "entry");
                ctx.temp_counter = 0;
                ctx.symbols = NULL; 
                
                // Setup Params allocation
                p = fn->params;
                int p_idx = 0;
                while(p) {
                    AlirValue *ptr = new_temp(&ctx, p->type);
                    emit(&ctx, mk_inst(ALIR_OP_ALLOCA, ptr, NULL, NULL));
                    alir_add_symbol(&ctx, p->name, ptr, p->type);
                    
                    // Store param val (assumed implicit registers p0, p1...)
                    char pname[16]; sprintf(pname, "p%d", p_idx++);
                    AlirValue *pval = alir_val_var(pname); 
                    emit(&ctx, mk_inst(ALIR_OP_STORE, NULL, pval, ptr));
                    
                    p = p->next;
                }
                
                ASTNode *stmt = fn->body;
                while(stmt) { alir_gen_stmt(&ctx, stmt); stmt = stmt->next; }
            }
        }
        curr = curr->next;
    }
    return ctx.module;
}
