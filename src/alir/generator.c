#include "alir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

// --- CONSTANT EVALUATION ---
// Helper to extract constant integer from AST node (Literals or Enum Members)
long alir_eval_constant_int(AlirCtx *ctx, ASTNode *node) {
    if (!node) return 0;
    
    if (node->type == NODE_LITERAL) {
        return ((LiteralNode*)node)->val.int_val;
    }
    
    // Handle Enum.Member Access
    if (node->type == NODE_MEMBER_ACCESS) {
        MemberAccessNode *ma = (MemberAccessNode*)node;
        VarType obj_t = sem_get_node_type(ctx->sem, ma->object);
        
        // If the object is an Enum Type or a Variable of Enum type
        // The Semantic phase should have resolved this type
        if (obj_t.base == TYPE_ENUM && obj_t.class_name) {
            long val = 0;
            if (alir_get_enum_value(ctx->module, obj_t.class_name, ma->member_name, &val)) {
                return val;
            }
        }
        
        // If it was a raw class access (e.g. MyEnum.Val), the object might be a VAR_REF
        // referring to the type name itself. The semantic check handles scope.
        // We rely on alir_get_enum_value looking up in the module registry.
    }
    
    // Handle Unary Minus on literals
    if (node->type == NODE_UNARY_OP) {
        UnaryOpNode *u = (UnaryOpNode*)node;
        if (u->op == TOKEN_MINUS) {
            return -alir_eval_constant_int(ctx, u->operand);
        }
    }
    
    return 0; // Fallback / Error
}

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
        } else if (curr->type == NODE_ENUM) {
            // Register Enum
            EnumNode *en = (EnumNode*)curr;
            AlirEnumEntry *head = NULL;
            AlirEnumEntry **tail = &head;
            
            EnumEntry *ent = en->entries;
            while(ent) {
                AlirEnumEntry *ae = calloc(1, sizeof(AlirEnumEntry));
                ae->name = strdup(ent->name);
                ae->value = ent->value;
                *tail = ae;
                tail = &ae->next;
                ent = ent->next;
            }
            alir_register_enum(ctx->module, en->name, head);
        } else if (curr->type == NODE_NAMESPACE) {
             alir_scan_and_register_classes(ctx, ((NamespaceNode*)curr)->body);
        }
        curr = curr->next;
    }
}

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
        
        // Evaluate Constant (Handles Literals AND Enums)
        sc->value = alir_eval_constant_int(ctx, cn->value);
        
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
        
        if (!cn->is_leak) emit(ctx, mk_inst(ALIR_OP_JUMP, NULL, alir_val_label(end_bb->label), NULL));
        
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
        emit(ctx, mk_inst(ALIR_OP_JUMP, NULL, alir_val_label(end_bb->label), NULL));
    }
    
    ctx->current_block = end_bb;
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
                else ptr = alir_gen_addr(ctx, (ASTNode*)an->target); // Handle implicit 'this' or global
                // Fallback for global if gen_addr returns global ref or similar logic inside
                if (!ptr) ptr = alir_val_var(an->name); 
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
                emit(ctx, mk_inst(ALIR_OP_JUMP, NULL, alir_val_label(body_bb->label), NULL));
                
                // Body
                ctx->current_block = body_bb;
                push_loop(ctx, cond_bb, end_bb);
                ASTNode *s = wn->body; while(s) { alir_gen_stmt(ctx, s); s=s->next; }
                pop_loop(ctx);
                
                // Fallthrough to Cond
                emit(ctx, mk_inst(ALIR_OP_JUMP, NULL, alir_val_label(cond_bb->label), NULL));

                // Cond
                ctx->current_block = cond_bb;
                AlirValue *cond = alir_gen_expr(ctx, wn->condition);
                AlirInst *br = mk_inst(ALIR_OP_CONDI, NULL, cond, alir_val_label(body_bb->label));
                br->args = malloc(sizeof(AlirValue*));
                br->args[0] = alir_val_label(end_bb->label);
                br->arg_count = 1;
                emit(ctx, br);
            } else {
                // While: Cond -> Body -> Cond / End
                emit(ctx, mk_inst(ALIR_OP_JUMP, NULL, alir_val_label(cond_bb->label), NULL));

                // Cond
                ctx->current_block = cond_bb;
                AlirValue *cond = alir_gen_expr(ctx, wn->condition);
                AlirInst *br = mk_inst(ALIR_OP_CONDI, NULL, cond, alir_val_label(body_bb->label));
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
                emit(ctx, mk_inst(ALIR_OP_JUMP, NULL, alir_val_label(cond_bb->label), NULL));
            }
            ctx->current_block = end_bb;
            break;
        }

        case NODE_LOOP: {
            LoopNode *ln = (LoopNode*)node;
            AlirBlock *body_bb = alir_add_block(ctx->current_func, "loop_body");
            AlirBlock *end_bb = alir_add_block(ctx->current_func, "loop_end");
            
            emit(ctx, mk_inst(ALIR_OP_JUMP, NULL, alir_val_label(body_bb->label), NULL));
            
            ctx->current_block = body_bb;
            push_loop(ctx, body_bb, end_bb); // Continue goes to start of body
            
            ASTNode *s = ln->body; while(s) { alir_gen_stmt(ctx, s); s=s->next; }
            pop_loop(ctx);
            emit(ctx, mk_inst(ALIR_OP_JUMP, NULL, alir_val_label(body_bb->label), NULL));
            
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
            
            emit(ctx, mk_inst(ALIR_OP_JUMP, NULL, alir_val_label(cond_bb->label), NULL));
            
            // Condition: ITER_VALID
            ctx->current_block = cond_bb;
            AlirValue *valid = new_temp(ctx, (VarType){TYPE_BOOL, 0});
            emit(ctx, mk_inst(ALIR_OP_ITER_VALID, valid, iter, NULL));
            
            AlirInst *br = mk_inst(ALIR_OP_CONDI, NULL, valid, alir_val_label(body_bb->label));
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
            emit(ctx, mk_inst(ALIR_OP_JUMP, NULL, alir_val_label(cond_bb->label), NULL));
            
            pop_loop(ctx);
            ctx->current_block = end_bb;
            break;
        }

        case NODE_BREAK:
            if (ctx->loop_break) emit(ctx, mk_inst(ALIR_OP_JUMP, NULL, alir_val_label(ctx->loop_break->label), NULL));
            break;
            
        case NODE_CONTINUE:
            if (ctx->loop_continue) emit(ctx, mk_inst(ALIR_OP_JUMP, NULL, alir_val_label(ctx->loop_continue->label), NULL));
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
            
            AlirInst *br = mk_inst(ALIR_OP_CONDI, NULL, cond, alir_val_label(then_bb->label));
            br->args = malloc(sizeof(AlirValue*));
            br->args[0] = alir_val_label(target_else->label);
            br->arg_count = 1;
            emit(ctx, br);
            
            ctx->current_block = then_bb;
            ASTNode *s = in->then_body; while(s){ alir_gen_stmt(ctx,s); s=s->next; }
            emit(ctx, mk_inst(ALIR_OP_JUMP, NULL, alir_val_label(merge_bb->label), NULL));
            
            if (in->else_body) {
                ctx->current_block = else_bb;
                s = in->else_body; while(s){ alir_gen_stmt(ctx,s); s=s->next; }
                emit(ctx, mk_inst(ALIR_OP_JUMP, NULL, alir_val_label(merge_bb->label), NULL));
            }
            
            ctx->current_block = merge_bb;
            break;
        }
    }
}

