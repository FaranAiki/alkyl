#include "alir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Loop Stack
void push_loop(AlirCtx *ctx, AlirBlock *cont, AlirBlock *brk) {
    AlirCtx *node = alir_alloc(ctx->module, sizeof(AlirCtx));
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
}

// Helper to check if an instruction is a block terminator
int is_terminator(AlirOpcode op) {
    return op == ALIR_OP_RET || 
           op == ALIR_OP_JUMP || 
           op == ALIR_OP_CONDI || 
           op == ALIR_OP_SWITCH || 
           op == ALIR_OP_YIELD ||
           op == ALIR_OP_PANIC;
}

// Helper to extract constant integer from AST node (Literals or Enum Members)
long alir_eval_constant_int(AlirCtx *ctx, ASTNode *node) {
    if (!node) return 0;
    
    // maybe don't do this (?)
    if (node->type == NODE_LITERAL) {
        return ((LiteralNode*)node)->val.int_val;
    }
    
    // Handle Enum.Member Access
    if (node->type == NODE_MEMBER_ACCESS) {
        MemberAccessNode *ma = (MemberAccessNode*)node;
        VarType obj_t = sem_get_node_type(ctx->sem, ma->object);
        
        if (obj_t.base == TYPE_ENUM && obj_t.class_name) {
            long val = 0;
            if (alir_get_enum_value(ctx->module, obj_t.class_name, ma->member_name, &val)) {
                return val;
            }
        }
    }
    
    // Handle Unary Minus on literals
    if (node->type == NODE_UNARY_OP) {
        UnaryOpNode *u = (UnaryOpNode*)node;
        if (u->op == TOKEN_MINUS) {
            return -alir_eval_constant_int(ctx, u->operand);
        }
    }

    // Handle Direct Variable References to Enum Members
    if (node->type == NODE_VAR_REF) {
       VarRefNode *vr = (VarRefNode*)node;
       VarType t = sem_get_node_type(ctx->sem, node);
       
       if (t.base == TYPE_ENUM && t.class_name) {
           long val = 0;
           if (alir_get_enum_value(ctx->module, t.class_name, vr->name, &val)) {
               return val;
           }
       }

       // Handle error identifiers (ErrSomething) declared via errnum or implicitly
       if (ctx->sem && ctx->sem->compiler_ctx && strncmp(vr->name, "Err", 3) == 0) {
           void *err_val = hashmap_get(&ctx->sem->compiler_ctx->error_table, vr->name);
           if (err_val) {
               return (long)(intptr_t)err_val;
           }
       }

       // Fallback global enum search for bare enum members inside switches
       AlirEnum *e = ctx->module->enums;
       while(e) {
           AlirEnumEntry *ent = e->entries;
           while(ent) {
               if (strcmp(ent->name, vr->name) == 0) return ent->value;
               ent = ent->next;
           }
           e = e->next;
       }
    }

    return -42; // Fallback / Error
}

ClassNode* find_class_node(ASTNode *root, const char *name) {
    ASTNode *curr = root;
    while(curr) {
        if (curr->type == NODE_CLASS && strcmp(((ClassNode*)curr)->name, name) == 0) return (ClassNode*)curr;
        if (curr->type == NODE_NAMESPACE) {
            ClassNode *cn = find_class_node(((NamespaceNode*)curr)->body, name);
            if (cn) return cn;
        }
        curr = curr->next;
    }
    return NULL;
}

void build_struct_fields(AlirCtx *ctx, ASTNode *root, ClassNode *cn, AlirStruct *st) {
    if (st->field_count != -1) return; // Already built
    
    int idx = 0;
    AlirField *head = NULL;
    AlirField **tail = &head;
    
    // 1. Inherit Fields from Parent Class
    if (cn->parent_name) {
        AlirStruct *parent_st = alir_find_struct(ctx->module, cn->parent_name);
        if (parent_st) {
            if (parent_st->field_count == -1) {
                ClassNode *pcn = find_class_node(root, cn->parent_name);
                if (pcn) build_struct_fields(ctx, root, pcn, parent_st);
            }
            AlirField *pf = parent_st->fields;
            while(pf) {
                AlirField *nf = alir_alloc(ctx->module, sizeof(AlirField));
                nf->name = alir_strdup(ctx->module, pf->name); 
                nf->type = pf->type;
                nf->index = idx++;
                
                *tail = nf;
                tail = &nf->next;
                pf = pf->next;
            }
        }
    }
    
    // 2. Inherit Fields from Traits
    for (int i = 0; i < cn->traits.count; i++) {
        AlirStruct *trait_st = alir_find_struct(ctx->module, cn->traits.names[i]);
        if (trait_st) {
            if (trait_st->field_count == -1) {
                ClassNode *tcn = find_class_node(root, cn->traits.names[i]);
                if (tcn) build_struct_fields(ctx, root, tcn, trait_st);
            }
            AlirField *tf = trait_st->fields;
            while(tf) {
                AlirField *nf = alir_alloc(ctx->module, sizeof(AlirField));
                nf->name = alir_strdup(ctx->module, tf->name);
                nf->type = tf->type;
                nf->index = idx++;
                
                *tail = nf;
                tail = &nf->next;
                tf = tf->next;
            }
        }
    }

    // 3. Current Class Fields
    ASTNode *mem = cn->members;
    while(mem) {
        if (mem->type == NODE_VAR_DECL) {
            VarDeclNode *vd = (VarDeclNode*)mem;
            AlirField *f = alir_alloc(ctx->module, sizeof(AlirField));
            f->name = alir_strdup(ctx->module, vd->name);
            f->type = vd->var_type;
            
            // [FIX] Decay inline arrays to pointers to prevent struct bloat and truncation crashes
            if (f->type.array_size > 0) {
                f->type.array_size = 0;
                f->type.ptr_depth++;
            }
            
            f->index = idx++;
            
            *tail = f;
            tail = &f->next;
        }
        mem = mem->next;
    }
    
    st->fields = head;
    st->field_count = idx;
}

void pass1_register(AlirCtx *ctx, ASTNode *n) {
    while(n) {
        if (n->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)n;
            alir_register_struct(ctx->module, cn->name, NULL, cn->is_union);
        } else if (n->type == NODE_ENUM) {
            EnumNode *en = (EnumNode*)n;
            AlirEnumEntry *head = NULL;
            AlirEnumEntry **tail = &head;
            
            EnumEntry *ent = en->entries;
            while(ent) {
                AlirEnumEntry *ae = alir_alloc(ctx->module, sizeof(AlirEnumEntry));
                ae->name = alir_strdup(ctx->module, ent->name);
                ae->value = ent->value;
                *tail = ae;
                tail = &ae->next;
                ent = ent->next;
            }
            alir_register_enum(ctx->module, en->name, head);
        } else if (n->type == NODE_NAMESPACE) {
            pass1_register(ctx, ((NamespaceNode*)n)->body);
        }
        n = n->next;
    }
}

void pass2_populate(AlirCtx *ctx, ASTNode *root, ASTNode *n) {
    while(n) {
        if (n->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)n;
            AlirStruct *st = alir_find_struct(ctx->module, cn->name);
            if (st) build_struct_fields(ctx, root, cn, st);
        } else if (n->type == NODE_NAMESPACE) {
            pass2_populate(ctx, root, ((NamespaceNode*)n)->body);
        }
        n = n->next;
    }
}

void alir_scan_and_register_classes(AlirCtx *ctx, ASTNode *root) {
    pass1_register(ctx, root);
    pass2_populate(ctx, root, root);
}


void alir_gen_switch(AlirCtx *ctx, SwitchNode *sn) {
    AlirValue *cond = alir_gen_expr(ctx, sn->condition);
    if (!cond) cond = alir_const_int(ctx->module, 0); // Safety net for unresolvable conditions

    AlirBlock *end_bb = alir_add_block(ctx->module, ctx->current_func, "switch_end");
    AlirBlock *default_bb = end_bb; 
    
    if (sn->default_case) default_bb = alir_add_block(ctx->module, ctx->current_func, "switch_default");

    AlirInst *sw = mk_inst(ctx->module, ALIR_OP_SWITCH, NULL, cond, alir_val_label(ctx->module, default_bb->label));
    sw->cases = NULL;
    AlirSwitchCase **tail = &sw->cases;

    ASTNode *c = sn->cases;
    while(c) {
        CaseNode *cn = (CaseNode*)c;
        AlirBlock *case_bb = alir_add_block(ctx->module, ctx->current_func, "case");
        
        // Handle multiple cases grouped in an array literal (e.g. case Ayam, Daging:)
        if (cn->value && cn->value->type == NODE_ARRAY_LIT) {
            ArrayLitNode *al = (ArrayLitNode*)cn->value;
            ASTNode *elem = al->elements;
            while(elem) {
                AlirSwitchCase *sc = alir_alloc(ctx->module, sizeof(AlirSwitchCase));
                sc->label = case_bb->label;
                sc->value = alir_eval_constant_int(ctx, elem);
                *tail = sc;
                tail = &sc->next;
                elem = elem->next;
            }
        } else {
            AlirSwitchCase *sc = alir_alloc(ctx->module, sizeof(AlirSwitchCase));
            sc->label = case_bb->label;
            sc->value = alir_eval_constant_int(ctx, cn->value);
            
            *tail = sc;
            tail = &sc->next;
        }
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
            if (sc_iter && strcmp(search->label, sc_iter->label) == 0) { case_bb = search; break; }
            search = search->next;
        }
        
        ctx->current_block = case_bb;
        push_loop(ctx, NULL, end_bb);
        
        ASTNode *stmt = cn->body;
        while(stmt) { alir_gen_stmt(ctx, stmt); stmt = stmt->next; }
        
        pop_loop(ctx);
        
        // Advance sc_iter correctly depending on if it was an array literal of multiple cases
        AlirSwitchCase *next_sc_iter = sc_iter;
        if (cn->value && cn->value->type == NODE_ARRAY_LIT) {
            ArrayLitNode *al = (ArrayLitNode*)cn->value;
            ASTNode *elem = al->elements;
            while(elem && next_sc_iter) {
                next_sc_iter = next_sc_iter->next;
                elem = elem->next;
            }
        } else {
            if (next_sc_iter) next_sc_iter = next_sc_iter->next;
        }
        
        AlirInst *tail = ctx->current_block->tail;
        if (!tail || !is_terminator(tail->op)) {
            if (!cn->is_leak) {
                emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, end_bb->label), NULL));
            } else {
                char *target_label = default_bb->label;
                if (next_sc_iter) target_label = next_sc_iter->label;
                emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, target_label), NULL));
            }
        }
        
        c = c->next;
        sc_iter = next_sc_iter;
    }
    
    if (sn->default_case) {
        ctx->current_block = default_bb;
        push_loop(ctx, NULL, end_bb);
        ASTNode *stmt = sn->default_case;
        while(stmt) { alir_gen_stmt(ctx, stmt); stmt = stmt->next; }
        pop_loop(ctx);
        
        AlirInst *tail = ctx->current_block->tail;
        if (!tail || !is_terminator(tail->op)) {
            emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, end_bb->label), NULL));
        }
    }
    
    ctx->current_block = end_bb;
}

// This is for the implicit constructor 
// TODO learn this
void alir_gen_implicit_constructor(AlirCtx *ctx, ClassNode *cn) {
    AlirFunction *af = alir_add_function(ctx->module, cn->name, (VarType){TYPE_VOID, 0}, 0);
    ctx->current_func = af;
    
    VarType this_t = {TYPE_CLASS, 1, alir_strdup(ctx->module, cn->name)};
    alir_func_add_param(ctx->module, ctx->current_func, "this", this_t);

    AlirStruct *st = alir_find_struct(ctx->module, cn->name);
    
    Parameter *p_head = NULL;
    Parameter **p_tail = &p_head;
    int p_count = 0;

    if (ctx->sem) {
        Parameter *p_this = arena_alloc_type(ctx->sem->compiler_ctx->arena, Parameter);
        p_this->name = arena_strdup(ctx->sem->compiler_ctx->arena, "this");
        p_this->type = this_t;
        *p_tail = p_this; p_tail = &p_this->next;
        p_count++;
    }

    if (st && !cn->is_union) {
        AlirField *f = st->fields;
        while (f) {
            alir_func_add_param(ctx->module, ctx->current_func, f->name, f->type);
            if (ctx->sem) {
                Parameter *p_f = arena_alloc_type(ctx->sem->compiler_ctx->arena, Parameter);
                p_f->name = arena_strdup(ctx->sem->compiler_ctx->arena, f->name);
                p_f->type = f->type;
                *p_tail = p_f; p_tail = &p_f->next;
            }
            p_count++;
            f = f->next;
        }
    }
    
    // [FIX] Register the implicit constructor in the class's semantic symbol table
    // so that call-site validation (sem_check_call) sees the correct parameter count.
    if (ctx->sem) {
        SemSymbol *class_sym = sem_symbol_lookup(ctx->sem, cn->name, NULL);
        if (class_sym && class_sym->kind == SYM_CLASS && class_sym->inner_scope) {
            SemSymbol *ctor_sym = arena_alloc_type(ctx->sem->compiler_ctx->arena, SemSymbol);
            memset(ctor_sym, 0, sizeof(SemSymbol));
            ctor_sym->name = arena_strdup(ctx->sem->compiler_ctx->arena, cn->name);
            ctor_sym->kind = SYM_FUNC;
            ctor_sym->type = (VarType){TYPE_VOID, 0};
            ctor_sym->params = p_head;
            ctor_sym->param_count = p_count;
            ctor_sym->next = class_sym->inner_scope->symbols;
            class_sym->inner_scope->symbols = ctor_sym;
        }
    }

    ctx->current_block = alir_add_block(ctx->module, ctx->current_func, "entry");
    
    AlirValue *this_ptr = new_temp(ctx, this_t);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, this_ptr, NULL, NULL));
    alir_add_symbol(ctx, "this", this_ptr, this_t);
    emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, alir_val_var(ctx->module, "p0"), this_ptr));

    if (st && !cn->is_union) {
        int param_idx = 1;
        AlirField *f = st->fields;
        while (f) {
            char p_name[64];
            snprintf(p_name, sizeof(p_name), "p%d", param_idx);
            AlirValue *arg_val = alir_val_var(ctx->module, p_name);
            arg_val->type = f->type;
            
            AlirValue *loaded_this = new_temp(ctx, this_t);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_LOAD, loaded_this, this_ptr, NULL));

            VarType ft = f->type; ft.ptr_depth++;
            AlirValue *field_ptr = new_temp(ctx, ft);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, field_ptr, loaded_this, alir_const_int(ctx->module, f->index)));
            emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, arg_val, field_ptr));
            
            param_idx++;
            f = f->next;
        }
    }

    emit(ctx, mk_inst(ctx->module, ALIR_OP_RET, NULL, NULL, NULL));
}

void alir_gen_function_def(AlirCtx *ctx, FuncDefNode *fn, const char *class_name) {
    if (fn->is_flux) {
        alir_gen_flux_def(ctx, fn, class_name);
        return;
    }

    char func_name[256];
    if (fn->mangled_name) {
        if (class_name) {
            char search_str[256];
            snprintf(search_str, sizeof(search_str), "_%s", fn->name);
            char *pos = strstr(fn->mangled_name, search_str);
            if (pos) {
                snprintf(func_name, sizeof(func_name), "%s%s", class_name, pos);
            } else {
                snprintf(func_name, sizeof(func_name), "%s", fn->mangled_name);
            }
        } else {
            snprintf(func_name, sizeof(func_name), "%s", fn->mangled_name);
        }
    } else {
        if (class_name) {
            if (strcmp(fn->name, "init") == 0 || strcmp(fn->name, class_name) == 0) {
                snprintf(func_name, sizeof(func_name), "%s", class_name);
            } else {
                snprintf(func_name, sizeof(func_name), "%s_%s", class_name, fn->name);
            }
        } else {
            snprintf(func_name, sizeof(func_name), "%s", fn->name);
        }
    }

    ctx->current_func = alir_add_function(ctx->module, func_name, fn->ret_type, 0);
    ctx->current_func->is_varargs = fn->is_varargs;
    ctx->current_func->is_extern = fn->is_extern;
    if (fn->cconv) ctx->current_func->cconv = alir_strdup(ctx->module, fn->cconv);

    if (class_name) {
        VarType this_t = {TYPE_CLASS, 1, alir_strdup(ctx->module, class_name)};
        alir_func_add_param(ctx->module, ctx->current_func, "this", this_t);
    }

    Parameter *p = fn->params;
    while(p) {
        alir_func_add_param(ctx->module, ctx->current_func, p->name, p->type);
        p = p->next;
    }

    if (!fn->has_body) return;

    ctx->current_block = alir_add_block(ctx->module, ctx->current_func, "entry");
    ctx->temp_counter = 0;
    ctx->symbols = NULL; 

    int p_idx = 0;

    if (class_name) {
        VarType this_t = {TYPE_CLASS, 1, alir_strdup(ctx->module, class_name)};
        
        char pname[16]; snprintf(pname, sizeof(pname), "p%d", p_idx++);
        AlirValue *pval = alir_val_var(ctx->module, pname);
        pval->type = this_t;
        
        // [FIX] Actually allocate a local pointer for `this` to preserve standard calling conventions
        AlirValue *ptr = new_temp(ctx, this_t);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, ptr, NULL, NULL));
        alir_add_symbol(ctx, "this", ptr, this_t);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, pval, ptr));
    }

    // For checking params
    p = fn->params;
    while(p) {
        AlirValue *ptr = new_temp(ctx, p->type);
        emit(ctx, mk_inst(ctx->module, ALIR_OP_ALLOCA, ptr, NULL, NULL));
        alir_add_symbol(ctx, p->name, ptr, p->type);
        
        char pname[16]; snprintf(pname, sizeof(pname), "p%d", p_idx++);
        AlirValue *pval = alir_val_var(ctx->module, pname); 
        pval->type = p->type;
        emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, pval, ptr));
        
        p = p->next;
    }
    
    ASTNode *stmt = fn->body;
    while(stmt) { alir_gen_stmt(ctx, stmt); stmt = stmt->next; }

    if (ctx->current_block) {
        AlirInst *tail = ctx->current_block->tail;
        int has_term = tail && is_terminator(tail->op);
        
        if (!has_term) {
            ctx->current_line = fn->base.line;
            ctx->current_col = fn->base.col;
            
            if (strcmp(func_name, "main") == 0) {
                emit(ctx, mk_inst(ctx->module, ALIR_OP_RET, NULL, alir_const_int(ctx->module, 0), NULL));
            } else if (fn->ret_type.base == TYPE_VOID || (class_name && (strcmp(fn->name, "init") == 0 || strcmp(fn->name, class_name) == 0))) {
                emit(ctx, mk_inst(ctx->module, ALIR_OP_RET, NULL, NULL, NULL));
            } else {
                // Fallback for non-void functions that missed a return
                // Emit a dummy return to keep IR valid
                AlirValue *dummy = NULL;
                if (is_integer(fn->ret_type)) dummy = alir_const_int(ctx->module, 0);
                else if (is_numeric(fn->ret_type)) dummy = alir_const_float(ctx->module, 0.0);
                else if (is_pointer(fn->ret_type)) dummy = alir_const_int(ctx->module, 0); // null
                
                emit(ctx, mk_inst(ctx->module, ALIR_OP_RET, NULL, dummy, NULL));
            }
        }
    }
}

// Emits inherited methods from parent and traits down to the derived class scope
void alir_gen_inherited_methods(AlirCtx *ctx, ASTNode *root, ClassNode *cn, const char *target_class) {
    if (!cn) return;
    
    // 1. Traverse Parent
    if (cn->parent_name) {
        ClassNode *pcn = find_class_node(root, cn->parent_name);
        if (pcn) {
            alir_gen_inherited_methods(ctx, root, pcn, target_class); // Deepest first
            
            ASTNode *mem = pcn->members;
            while (mem) {
                if (mem->type == NODE_FUNC_DEF) {
                    FuncDefNode *fn = (FuncDefNode*)mem;
                    if (strcmp(fn->name, pcn->name) != 0 && strcmp(fn->name, "init") != 0) {
                        ClassNode *tcn = find_class_node(root, target_class);
                        int is_overridden = 0;
                        if (tcn) {
                            ASTNode *tmem = tcn->members;
                            while(tmem) {
                                if (tmem->type == NODE_FUNC_DEF && strcmp(((FuncDefNode*)tmem)->name, fn->name) == 0) {
                                    is_overridden = 1; break;
                                }
                                tmem = tmem->next;
                            }
                        }
                        if (!is_overridden) {
                            alir_gen_function_def(ctx, fn, target_class);
                        }
                    }
                }
                mem = mem->next;
            }
        }
    }
    
    // 2. Traverse Traits
    for (int i = 0; i < cn->traits.count; i++) {
        ClassNode *tcn = find_class_node(root, cn->traits.names[i]);
        if (tcn) {
            alir_gen_inherited_methods(ctx, root, tcn, target_class);
            
            ASTNode *mem = tcn->members;
            while (mem) {
                if (mem->type == NODE_FUNC_DEF) {
                    FuncDefNode *fn = (FuncDefNode*)mem;
                    if (strcmp(fn->name, tcn->name) != 0 && strcmp(fn->name, "init") != 0) {
                        ClassNode *target_node = find_class_node(root, target_class);
                        int is_overridden = 0;
                        if (target_node) {
                            ASTNode *tmem = target_node->members;
                            while(tmem) {
                                if (tmem->type == NODE_FUNC_DEF && strcmp(((FuncDefNode*)tmem)->name, fn->name) == 0) {
                                    is_overridden = 1; break;
                                }
                                tmem = tmem->next;
                            }
                        }
                        if (!is_overridden) {
                            alir_gen_function_def(ctx, fn, target_class);
                        }
                    }
                }
                mem = mem->next;
            }
        }
    }
}

// Deeply scan AST for Class/Methods and Standard Functions
void alir_gen_functions_recursive(AlirCtx *ctx, ASTNode *root) {
    ASTNode *curr = root;
    while(curr) {
        if (curr->type == NODE_FUNC_DEF) {
            FuncDefNode *fn = (FuncDefNode*)curr;
            if (!fn->is_macro) {
                alir_gen_function_def(ctx, fn, NULL);
            }
        } else if (curr->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)curr;
            int has_constructor = 0;
            
            ASTNode *mem = cn->members;
            while(mem) {
                if (mem->type == NODE_FUNC_DEF) {
                    FuncDefNode *fn = (FuncDefNode*)mem;
                    if (strcmp(fn->name, cn->name) == 0 || strcmp(fn->name, "init") == 0) {
                        has_constructor = 1;
                    }
                    alir_gen_function_def(ctx, fn, cn->name);
                }
                mem = mem->next;
            }

            // Generate Inherited and Traited Methods for this specific Class
            alir_gen_inherited_methods(ctx, root, cn, cn->name);
            
            // Emit an implicit constructor if the user hasn't explicitly supplied `init`
            if (!has_constructor) {
                alir_gen_implicit_constructor(ctx, cn);
            }
        } else if (curr->type == NODE_NAMESPACE) {
            alir_gen_functions_recursive(ctx, ((NamespaceNode*)curr)->body);
        } else if (curr->type == NODE_META || curr->type == NODE_POSTMETA) {
            alir_gen_stmt(ctx, curr);
        }
        curr = curr->next;
    }
}

AlirModule* alir_generate(SemanticCtx *sem, ASTNode *root) {
    AlirCtx ctx;
    memset(&ctx, 0, sizeof(AlirCtx));
    ctx.sem = sem; 
    ctx.module = alir_create_module(sem ? sem->compiler_ctx : NULL, "main_module");

    if (sem) {
        ctx.module->src = sem->current_source;
        ctx.module->filename = sem->current_filename;
    }
    
    // 1. SCAN AND REGISTER CLASSES & ENUMS
    alir_scan_and_register_classes(&ctx, root);
    
    // 2. GEN FUNCTIONS (Recursively to handle classes & namespaces)
    alir_gen_functions_recursive(&ctx, root);
    
    return ctx.module;
}
