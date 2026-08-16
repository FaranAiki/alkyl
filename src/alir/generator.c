/**
 * @file generator.c
 * @brief ALIR generation implementation.
 */
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
               if (streq_lit(ent->name, vr->name)) return ent->value;
               ent = ent->next;
           }
           e = e->next;
       }
    }

    return -42; // Fallback / Error
}

void build_struct_fields(AlirCtx *ctx, ClassNode *cn, AlirStruct *st) {
    if (st->field_count != -1) return; // Already built

    int idx = 0;
    AlirField *head = NULL;
    AlirField **tail = &head;

    // 1. Inherit Fields from Parent Class
    if (cn->parent_name) {
        AlirStruct *parent_st = alir_find_struct(ctx->module, cn->parent_name);
        if (parent_st) {
            if (parent_st->field_count == -1) {
                ClassNode *pcn = hashmap_get(&ctx->class_map, cn->parent_name);
                if (pcn) build_struct_fields(ctx, pcn, parent_st);
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
                ClassNode *tcn = hashmap_get(&ctx->class_map, cn->traits.names[i]);
                if (tcn) build_struct_fields(ctx, tcn, trait_st);
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

void pass1_register(AlirCtx *ctx, ASTNode *n, const char *current_ns) {
    while(n) {
        if (n->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)n;
            debug_alir("Visiting class %s\n", cn->name);
            char *fqn = cn->name;
            if (current_ns && strlen(current_ns) > 0) {
                char buf[512];
                snprintf(buf, sizeof(buf), "%s.%s", current_ns, cn->name);
                fqn = alir_strdup(ctx->module, buf);
            }
            alir_register_struct(ctx->module, fqn, NULL, cn->is_union);
            hashmap_put(&ctx->class_map, cn->name, cn);
        } else if (n->type == NODE_ENUM) {
            EnumNode *en = (EnumNode*)n;
            AlirEnumEntry *head = NULL;
            AlirEnumEntry **tail = &head;

            EnumEntry *ent = en->entries;
            while(ent) {
                AlirEnumEntry *ae = alir_alloc(ctx->module, sizeof(AlirEnumEntry));
                ae->name = alir_strdup(ctx->module, ent->name);
                ae->value = ent->value;
                ae->next = NULL;
                *tail = ae;
                tail = &ae->next;
                ent = ent->next;
            }
            alir_register_enum(ctx->module, en->name, head);
        } else if (n->type == NODE_IMPORT) {
            ImportNode *in = (ImportNode*)n;
            if (in->resolved_body) {
                pass1_register(ctx, in->resolved_body, current_ns);
            }
        } else if (n->type == NODE_IMPORT_EXPR) {
            ImportExprNode *ie = (ImportExprNode*)n;
            if (ie->resolved_body) {
                pass1_register(ctx, ie->resolved_body, current_ns);
            }
        } else if (n->type == NODE_VAR_DECL) {
            VarDeclNode *vd = (VarDeclNode*)n;
            if (vd->initializer && vd->initializer->type == NODE_IMPORT_EXPR) {
                ImportExprNode *ie = (ImportExprNode*)vd->initializer;
                if (ie->resolved_body) {
                    pass1_register(ctx, ie->resolved_body, current_ns);
                }
            }
        } else if (n->type == NODE_NAMESPACE) {
            NamespaceNode *ns = (NamespaceNode*)n;
            char *next_ns = ns->name;
            if (current_ns && strlen(current_ns) > 0) {
                char buf[512];
                snprintf(buf, sizeof(buf), "%s.%s", current_ns, ns->name);
                next_ns = alir_strdup(ctx->module, buf);
            }
            pass1_register(ctx, ns->body, next_ns);
        }
        n = n->next;
    }
}

void pass2_populate(AlirCtx *ctx, ASTNode *root, ASTNode *n, const char *current_ns) {
    while(n) {
        if (n->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)n;
            char *fqn = cn->name;
            if (current_ns && strlen(current_ns) > 0) {
                char buf[512];
                snprintf(buf, sizeof(buf), "%s.%s", current_ns, cn->name);
                fqn = alir_strdup(ctx->module, buf);
            }
            AlirStruct *st = alir_find_struct(ctx->module, fqn);
            if (st) build_struct_fields(ctx, cn, st);
        } else if (n->type == NODE_NAMESPACE) {
            NamespaceNode *ns = (NamespaceNode*)n;
            char *next_ns = ns->name;
            if (current_ns && strlen(current_ns) > 0) {
                char buf[512];
                snprintf(buf, sizeof(buf), "%s.%s", current_ns, ns->name);
                next_ns = alir_strdup(ctx->module, buf);
            }
            pass2_populate(ctx, root, ns->body, next_ns);
        } else if (n->type == NODE_IMPORT) {
            ImportNode *in = (ImportNode*)n;
            if (in->resolved_body) {
                pass2_populate(ctx, root, in->resolved_body, current_ns);
            }
        } else if (n->type == NODE_IMPORT_EXPR) {
            ImportExprNode *ie = (ImportExprNode*)n;
            if (ie->resolved_body) {
                pass2_populate(ctx, root, ie->resolved_body, current_ns);
            }
        } else if (n->type == NODE_VAR_DECL) {
            VarDeclNode *vd = (VarDeclNode*)n;
            if (vd->initializer && vd->initializer->type == NODE_IMPORT_EXPR) {
                ImportExprNode *ie = (ImportExprNode*)vd->initializer;
                if (ie->resolved_body) {
                    pass2_populate(ctx, root, ie->resolved_body, current_ns);
                }
            }
        }
        n = n->next;
    }
}

void alir_scan_and_register_classes(AlirCtx *ctx, ASTNode *root) {
    hashmap_init(&ctx->class_map, ctx->module->compiler_ctx ? ctx->module->compiler_ctx->arena : NULL, 64);
    pass1_register(ctx, root, NULL);
    pass2_populate(ctx, root, root, NULL);
}


void alir_gen_switch(AlirCtx *ctx, SwitchNode *sn) {
    AlirValue *cond = alir_gen_expr(ctx, sn->condition);
    if (!cond) cond = alir_const_int(ctx->module, 0); // Safety net for unresolvable conditions

    AlirBlock *end_bb = alir_add_block(ctx->module, ctx->current_func, "switch_end");
    AlirBlock *default_bb = end_bb;

    if (sn->default_case) default_bb = alir_add_block(ctx->module, ctx->current_func, "switch_default");

    // Build case blocks first so labels are available for jumps
    typedef struct { AlirBlock *bb; long value; } CaseBlock;
    CaseBlock case_blocks[64];
    int num_cases = 0;
    ASTNode *c = sn->cases;
    while(c) {
        CaseNode *cn = (CaseNode*)c;
        AlirBlock *case_bb = alir_add_block(ctx->module, ctx->current_func, "case");

        // Handle multiple cases grouped in an array literal (e.g. case 1, 2:)
        if (cn->value && cn->value->type == NODE_ARRAY_LIT) {
            ArrayLitNode *al = (ArrayLitNode*)cn->value;
            ASTNode *elem = al->elements;
            while(elem && num_cases < 64) {
                case_blocks[num_cases].bb = case_bb;
                case_blocks[num_cases].value = alir_eval_constant_int(ctx, elem);
                num_cases++;
                elem = elem->next;
            }
        } else {
            if (num_cases < 64) {
                case_blocks[num_cases].bb = case_bb;
                case_blocks[num_cases].value = alir_eval_constant_int(ctx, cn->value);
                num_cases++;
            }
        }
        c = c->next;
    }

    // Emit cascading if/else chain: eq + conditional branch to case, else check next
    // Pre-create all fallthrough check blocks so we can use their actual unique labels
    AlirBlock *check_blocks[64];
    for (int i = 1; i < num_cases; i++) {
        char hint[64];
        snprintf(hint, sizeof(hint), "switch_check_%d", num_cases - i);
        check_blocks[i] = alir_add_block(ctx->module, ctx->current_func, hint);
    }

    for (int i = 0; i < num_cases; i++) {
        AlirValue *cmp = new_temp(ctx, (VarType){TYPE_BOOL, 0});
        emit(ctx, mk_inst(ctx->module, ALIR_OP_EQ, cmp, cond, alir_const_int(ctx->module, case_blocks[i].value)));

        AlirValue *target = alir_val_label(ctx->module, case_blocks[i].bb->label);
        AlirValue *fallthrough;
        if (i + 1 < num_cases) {
            fallthrough = alir_val_label(ctx->module, check_blocks[i + 1]->label);
        } else {
            fallthrough = alir_val_label(ctx->module, default_bb->label);
        }

        AlirInst *br = mk_inst(ctx->module, ALIR_OP_CONDI, NULL, cmp, target);
        br->args = alir_alloc(ctx->module, sizeof(AlirValue*));
        br->args[0] = fallthrough;
        br->arg_count = 1;
        emit(ctx, br);

        if (i + 1 < num_cases) {
            ctx->current_block = check_blocks[i + 1];
        }
    }

    c = sn->cases;
    int case_idx = 0;
    while(c) {
        CaseNode *cn = (CaseNode*)c;
        AlirBlock *case_bb = NULL;
        if (case_idx < num_cases) {
            case_bb = case_blocks[case_idx].bb;
        }

        if (case_bb) {
            ctx->current_block = case_bb;
            push_loop(ctx, NULL, end_bb);

            ASTNode *stmt = cn->body;
            while(stmt) { alir_gen_stmt(ctx, stmt); stmt = stmt->next; }
            pop_loop(ctx);

            AlirInst *tail = ctx->current_block->tail;
            if (!tail || !is_terminator(tail->op)) {
                if (!cn->is_leak) {
                    emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, end_bb->label), NULL));
                } else {
                    char *target_label = default_bb->label;
                    emit(ctx, mk_inst(ctx->module, ALIR_OP_JUMP, NULL, alir_val_label(ctx->module, target_label), NULL));
                }
            }
        }

        c = c->next;
        case_idx++;
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
void alir_gen_implicit_constructor(AlirCtx *ctx, ClassNode *cn, const char *fqn) {
    AlirFunction *af = alir_add_function(ctx->module, fqn, (VarType){TYPE_VOID, 0}, 0);
    ctx->current_func = af;

    VarType this_t = {TYPE_CLASS, 1, alir_strdup(ctx->module, fqn)};
    alir_func_add_param(ctx->module, ctx->current_func, "this", this_t);

    AlirStruct *st = alir_find_struct(ctx->module, fqn);

    Parameter *p_head = NULL;
    Parameter **p_tail = &p_head;
    if (ctx->sem) {
        Parameter *p_this = arena_alloc_type(ctx->sem->compiler_ctx->arena, Parameter);
        p_this->name = arena_strdup(ctx->sem->compiler_ctx->arena, "this");
        p_this->type = this_t;
        *p_tail = p_this; p_tail = &p_this->next;
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
            f = f->next;
        }
    }

    // Removed: ALIR should not mutate the semantic symbol table with an implicit constructor
    // because it includes the hidden 'this' parameter, breaking argument counts in subsequent
    // REPL commands when sem_check_call_args sees it.

    ctx->current_block = alir_add_block(ctx->module, ctx->current_func, "entry");

    AlirValue *p0 = alir_val_var(ctx->module, "p0");
    p0->type = this_t;
    alir_add_symbol(ctx, "this", p0, this_t);

    if (st && !cn->is_union) {
        int param_idx = 1;
        AlirField *f = st->fields;
        while (f) {
            char p_name[64];
            snprintf(p_name, sizeof(p_name), "p%d", param_idx);
            AlirValue *arg_val = alir_val_var(ctx->module, p_name);
            arg_val->type = f->type;

            VarType ft = f->type; ft.ptr_depth++;
            AlirValue *field_ptr = new_temp(ctx, ft);
            emit(ctx, mk_inst(ctx->module, ALIR_OP_GET_PTR, field_ptr, p0, alir_const_int(ctx->module, f->index)));
            emit(ctx, mk_inst(ctx->module, ALIR_OP_STORE, NULL, arg_val, field_ptr));

            param_idx++;
            f = f->next;
        }
    }

    emit(ctx, mk_inst(ctx->module, ALIR_OP_RET, NULL, NULL, NULL));
}

// Emits inherited methods from parent and traits down to the derived class scope
void alir_gen_inherited_methods(AlirCtx *ctx, ClassNode *cn, const char *target_class_fqn, ClassNode *target_node) {
    if (!cn) return;

    HashMap target_methods;
    hashmap_init(&target_methods, ctx->module->compiler_ctx ? ctx->module->compiler_ctx->arena : NULL, 16);

    if (target_node) {
        ASTNode *tmem = target_node->members;
        while (tmem) {
            if (tmem->type == NODE_FUNC_DEF) {
                hashmap_put(&target_methods, ((FuncDefNode*)tmem)->name, (void*)1);
            }
            tmem = tmem->next;
        }
    }

    // 1. Traverse Parent
    if (cn->parent_name) {
        ClassNode *pcn = hashmap_get(&ctx->class_map, cn->parent_name);
        if (pcn) {
            alir_gen_inherited_methods(ctx, pcn, target_class_fqn, target_node); // Deepest first

            ASTNode *mem = pcn->members;
            while (mem) {
                if (mem->type == NODE_FUNC_DEF) {
                    FuncDefNode *fn = (FuncDefNode*)mem;
                    if (!streq_lit(fn->name, pcn->name) && !streq_lit(fn->name, "init")) {
                        int is_overridden = 0;
                        if (target_node && hashmap_has(&target_methods, fn->name)) {
                            is_overridden = 1;
                        }
                        if (!is_overridden) {
                            alir_gen_function_def(ctx, fn, target_class_fqn);
                        }
                    }
                }
                mem = mem->next;
            }
        }
    }

    // 2. Traverse Traits
    for (int i = 0; i < cn->traits.count; i++) {
        ClassNode *tcn = hashmap_get(&ctx->class_map, cn->traits.names[i]);
        if (tcn) {
            alir_gen_inherited_methods(ctx, tcn, target_class_fqn, target_node);

            ASTNode *mem = tcn->members;
            while (mem) {
                if (mem->type == NODE_FUNC_DEF) {
                    FuncDefNode *fn = (FuncDefNode*)mem;
                    if (!streq_lit(fn->name, tcn->name) && !streq_lit(fn->name, "init")) {
                        int is_overridden = 0;
                        if (target_node && hashmap_has(&target_methods, fn->name)) {
                            is_overridden = 1;
                        }
                        if (!is_overridden) {
                            alir_gen_function_def(ctx, fn, target_class_fqn);
                        }
                    }
                }
                mem = mem->next;
            }
        }
    }
}

// Deeply scan AST for Class/Methods and Standard Functions
void alir_gen_functions_recursive(AlirCtx *ctx, ASTNode *root, const char *current_ns) {
    ASTNode *curr = root;
    while(curr) {
        if (curr->type == NODE_FUNC_DEF) {
            FuncDefNode *fn = (FuncDefNode*)curr;
            debug_alir("Found func_def %s\n", fn->name);
            if (!fn->is_macro) {
                alir_gen_function_def(ctx, fn, fn->class_name);
            }
        } else if (curr->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)curr;
            int has_constructor = 0;

            const char *fqn = cn->name;
            if (current_ns && strlen(current_ns) > 0) {
                char buf[512];
                snprintf(buf, sizeof(buf), "%s.%s", current_ns, cn->name);
                fqn = alir_strdup(ctx->module, buf);
            }

            ASTNode *mem = cn->members;
            while(mem) {
                if (mem->type == NODE_FUNC_DEF) {
                    FuncDefNode *fn = (FuncDefNode*)mem;
                    if (streq_lit(fn->name, cn->name) || streq_lit(fn->name, "init")) {
                        has_constructor = 1;
                    }
                    if (!fn->is_macro) {
                        alir_gen_function_def(ctx, fn, fqn);
                    }
                }
                mem = mem->next;
            }

            // Generate Inherited and Traited Methods for this specific Class
            alir_gen_inherited_methods(ctx, cn, fqn, cn);

            // Emit an implicit constructor if the user hasn't explicitly supplied `init`
            if (!has_constructor) {
                alir_gen_implicit_constructor(ctx, cn, fqn);
            }
        } else if (curr->type == NODE_NAMESPACE) {
            NamespaceNode *ns = (NamespaceNode*)curr;
            debug_alir("Found namespace %s\n", ns->name);
            const char *next_ns = ns->name;
            if (current_ns && strlen(current_ns) > 0) {
                char buf[512];
                snprintf(buf, sizeof(buf), "%s.%s", current_ns, ns->name);
                next_ns = alir_strdup(ctx->module, buf);
            }
            
            alir_gen_functions_recursive(ctx, ns->body, next_ns);
        } else if (curr->type == NODE_IMPORT) {
            ImportNode *in = (ImportNode*)curr;
            if (in->resolved_body) {
                alir_gen_functions_recursive(ctx, in->resolved_body, current_ns);
            }
        } else if (curr->type == NODE_IMPORT_EXPR) {
            ImportExprNode *ie = (ImportExprNode*)curr;
            if (ie->resolved_body) {
                alir_gen_functions_recursive(ctx, ie->resolved_body, current_ns);
            }
        } else if (curr->type == NODE_VAR_DECL) {
            VarDeclNode *vd = (VarDeclNode*)curr;
            if (vd->initializer) {
                if (vd->initializer->type == NODE_IMPORT_EXPR) {
                    ImportExprNode *ie = (ImportExprNode*)vd->initializer;
                    if (ie->resolved_body) {
                        alir_gen_functions_recursive(ctx, ie->resolved_body, current_ns);
                    }
                }
                debug_alir("Found top-level VAR_DECL %s\n", vd->name);
                AlirGlobal *g = alir_alloc(ctx->module, sizeof(AlirGlobal));
                g->name = alir_strdup(ctx->module, vd->name);
                g->type = vd->var_type;
                g->next = ctx->module->globals;
                ctx->module->globals = g;
            }
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
    hashmap_init(&ctx.const_fold_map, ctx.module->compiler_ctx ? ctx.module->compiler_ctx->arena : NULL, 64);
    hashmap_init(&ctx.symbol_map, ctx.module->compiler_ctx ? ctx.module->compiler_ctx->arena : NULL, 128);

    if (sem) {
        ctx.module->src = sem->current_source;
        ctx.module->filename = sem->current_filename;
    }

    // 1. SCAN AND REGISTER CLASSES & ENUMS
    alir_scan_and_register_classes(&ctx, root);

    debug_alir("DEBUG_PASS1_END: struct list:\n");
    fflush(stdout);
    AlirStruct *ds = ctx.module->structs;
    while (ds) {
        printf(" - %s fields: %d\n", ds->name, ds->field_count);
        fflush(stdout);
        ds = ds->next;
    }

    // 1.5. FOLD TOP-LEVEL CONST DECLARATIONS WITH CONSTANT INITIALIZERS
    scan_and_fold_consts(&ctx, root);

    // 2. GEN FUNCTIONS (Recursively to handle classes & namespaces)
    alir_gen_functions_recursive(&ctx, root, NULL);

    return ctx.module;
}
