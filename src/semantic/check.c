#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../parser/c_parser.h"
#include "../parser/link.h"

SemSymbol* lookup_local_symbol(SemanticCtx *ctx, const char *name) {
    if (!ctx->current_scope) return NULL;
    SemSymbol *sym = ctx->current_scope->symbols;
    while (sym) {
        if (streq_lit(sym->name, name)) return sym;
        sym = sym->next;
    }
    return NULL;
}

int sem_is_constant_nonzero(ASTNode* node) {
    if (!node) return 0;
    if (node->type == NODE_LITERAL) {
        LiteralNode* lit = (LiteralNode*)node;
        switch (lit->var_type.base) {
            case TYPE_INT: return lit->val.int_val != 0;
            case TYPE_DOUBLE: return lit->val.double_val != 0.0;
            case TYPE_SINGLE: return lit->val.single_val != 0.0f;
            case TYPE_LONG: return lit->val.long_val != 0;
            case TYPE_CHAR: return lit->val.char_val != 0;
            case TYPE_BOOL: return lit->val.int_val != 0;
            default: return 0;
        }
    } else if (node->type == NODE_CAST) {
        CastNode* cn = (CastNode*)node;
        return sem_is_constant_nonzero(cn->operand);
    } else if (node->type == NODE_UNARY_OP) {
        UnaryOpNode* un = (UnaryOpNode*)node;
        if (un->op == TOKEN_MINUS || un->op == TOKEN_PLUS) {
            return sem_is_constant_nonzero(un->operand);
        }
    }
    return 0;
}

void sem_insert_implicit_cast(SemanticCtx *ctx, ASTNode **node_ptr, VarType target_type) {
    if (!node_ptr || !*node_ptr) return;
    VarType current = sem_get_node_type(ctx, *node_ptr);

    if (current.base == target_type.base && current.ptr_depth == target_type.ptr_depth) return;
    if (current.base == TYPE_UNKNOWN || target_type.base == TYPE_UNKNOWN) return;
    if (target_type.base == TYPE_VOID) return;

    CastNode *cast = arena_alloc_type(ctx->compiler_ctx->arena, CastNode);
    memset(cast, 0, sizeof(CastNode));
    cast->base.type = NODE_CAST;
    cast->base.line = (*node_ptr)->line;
    cast->base.col = (*node_ptr)->col;

    cast->base.next = (*node_ptr)->next;
    (*node_ptr)->next = NULL;

    cast->var_type = target_type;
    cast->operand = *node_ptr;

    sem_set_node_type(ctx, (ASTNode*)cast, target_type);

    *node_ptr = (ASTNode*)cast;
}

// TODO split this into
// multiple functions
void sem_scan_top_level(SemanticCtx *ctx, ASTNode *node) {
    if (!ctx->compiler_ctx || !ctx->compiler_ctx->arena) return;

    // TODO change this into switch instead of if-else
    while (node) {
        if (node->filename) ctx->current_filename = node->filename;
        ctx->current_node = node;
        if (node->type == NODE_FUNC_DEF) {
            FuncDefNode *fd = (FuncDefNode*)node;
            // Register the error set attached via `errnum [...]` and mark the
            // function's return type as tainted.
            if (fd->has_errnum && ctx->compiler_ctx) {
                fd->ret_type.is_tainted = 1;
                for (int i = 0; i < fd->num_err; i++) {
                    const char *name = fd->err_names[i];
                    if (!hashmap_get(&ctx->compiler_ctx->error_table, name)) {
                        int id = ctx->compiler_ctx->next_error_id++;
                        hashmap_put(&ctx->compiler_ctx->error_table, name, (void*)(intptr_t)(id + 1));
                    }
                }
            }
            sem_symbolic_func_def(ctx, node);
        }
        else if (node->type == NODE_VAR_DECL) {
            sem_symbolic_var_decl(ctx, node);
        }
        else if (node->type == NODE_IMPORT) {
            ImportNode *in = (ImportNode*)node;
            if (in->resolved_body) {
                sem_scan_top_level(ctx, in->resolved_body);
            } else if (in->path) {
                Lexer l;
                Parser p;
                memset(&l, 0, sizeof(l));
                memset(&p, 0, sizeof(p));
                l.ctx = ctx->compiler_ctx;
                if (ctx->current_filename && ctx->current_filename[0]) {
                    l.filename = (char*)ctx->current_filename;
                }
                parser_init(&p, &l, NULL);
                p.ctx = ctx->compiler_ctx;
                
                ASTNode *imported = parse_import_internal(&p, in->path);
                if (imported) {
                    sem_scan_top_level(ctx, imported);
                }
            }
        }
        else if (node->type == NODE_IMPORT_EXPR) {
            ImportExprNode *ie = (ImportExprNode*)node;
            if (ie->path) {
                if (ie->header == HEADER_C) {
                    SemSymbol *ns_sym = sem_symbol_lookup(ctx, ie->path, NULL);
                    if (!ns_sym || ns_sym->kind != SYM_NAMESPACE) {
                        if (ie->resolved_body) {
                            VarType ns_type = {TYPE_NAMESPACE, 0, arena_strdup(ctx->compiler_ctx->arena, ie->path), 0, 0, NULL, NULL, 0, 0, 0, 0};
                            ns_sym = sem_symbol_add(ctx, ie->path, SYM_NAMESPACE, ns_type);
                            SemScope *ns_scope = arena_alloc_type(ctx->compiler_ctx->arena, SemScope);
                            memset(ns_scope, 0, sizeof(SemScope));
                            ns_scope->symbols = NULL;
                            ns_scope->symbol_map = arena_alloc_type(ctx->compiler_ctx->arena, HashMap);
                            hashmap_init((HashMap*)ns_scope->symbol_map, ctx->compiler_ctx->arena, 16);
                            ns_scope->parent = ctx->current_scope;
                            ns_scope->is_function_scope = 0;
                            ns_scope->is_class_scope = 0;
                            ns_sym->inner_scope = ns_scope;
                            SemScope *old_scope = ctx->current_scope;
                            ctx->current_scope = ns_scope;
                            sem_scan_top_level(ctx, ie->resolved_body);
                            ctx->current_scope = old_scope;
                        }
                    }
                } else {
                    SemSymbol *ns_sym = sem_symbol_lookup(ctx, ie->path, NULL);
                    if (!ns_sym || ns_sym->kind != SYM_NAMESPACE) {
                        if (ie->resolved_body) {
                            sem_scan_top_level(ctx, ie->resolved_body);
                            ASTNode *curr = ie->resolved_body;
                            while (curr) { 
                                debug_semantic("CHECKING IMPORT NODE: type=%d\n", curr->type);
                                sem_check_node(ctx, curr); 
                                curr = curr->next; 
                            }
                        }
                        ns_sym = sem_symbol_lookup(ctx, ie->path, NULL);
                    }
                }
            }
        }
        else if (node->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)node;
            
            if (cn->is_extended) {
                if (!cn->is_method_class) {
                    sem_warning(ctx, node, "try extended method instead of extended as it can cause memory issue");
                }
                
                SemSymbol *sym = sem_symbol_lookup(ctx, cn->name, NULL);
                if (!sym) {
                    // Try to create primitive class wrapper if it's a primitive name
                    if (streq_lit(cn->name, "int") || streq_lit(cn->name, "char") || streq_lit(cn->name, "bool") || streq_lit(cn->name, "single") || streq_lit(cn->name, "double")) {
                        VarType type_class = {TYPE_CLASS, 0, arena_strdup(ctx->compiler_ctx->arena, cn->name), 0, 0, NULL, NULL, 0, 0, 0, 0, 0};
                        sym = sem_symbol_add(ctx, cn->name, SYM_CLASS, type_class);
                    } else {
                        sem_error(ctx, node, "Cannot extend non-existent class '%s'", cn->name);
                        node = node->next;
                        continue; // skip
                    }
                }
                
                if (sym->is_is_a == IS_A_FINAL) {
                    sem_error(ctx, node, "Cannot extend final class '%s'", cn->name);
                    node = node->next;
                    continue; // skip
                }
                
                // Inherit class properties if not explicitly a method class (to simulate copy behavior for normal extended)
                if (!cn->is_method_class) {
                    // "copy" or basically makes everything that is defined in the Vector "local" - handled by scanning members into the same scope
                }
                
                sem_scan_class_members(ctx, cn, sym);
            } else {
                VarType type_class = {TYPE_CLASS, 0, arena_strdup(ctx->compiler_ctx->arena, cn->name), 0, 0, NULL, NULL, 0, 0, 0, cn->is_tainted, 0};
                SemSymbol *sym = sem_symbol_add(ctx, cn->name, SYM_CLASS, type_class);
                sym->is_is_a = cn->is_is_a;
                sym->is_has_a = cn->is_has_a;
                sym->is_pure = cn->is_pure && !cn->is_extern;
                sym->must_pure = cn->has_explicit_pure;
                sym->is_union = cn->is_union;
                sym->node_ptr = node;
                if (cn->parent_name) {
                    sym->parent_name = arena_strdup(ctx->compiler_ctx->arena, cn->parent_name);
                }
                if (cn->traits.count > 0) {
                    sym->trait_count = cn->traits.count;
                    sym->traits = arena_alloc(ctx->compiler_ctx->arena, sizeof(char*) * sym->trait_count);
                    for (int i = 0; i < sym->trait_count; i++) {
                        sym->traits[i] = arena_strdup(ctx->compiler_ctx->arena, cn->traits.names[i]);
                    }
                }
                sem_scan_class_members(ctx, cn, sym);
            }
        }

        else if (node->type == NODE_STRUCT) {

        }
        else if (node->type == NODE_ENUM) {
            sem_symbolic_node_enum(ctx, node);
        }
        else if (node->type == NODE_ERRNUM) {
            extern void sem_symbolic_node_errnum(SemanticCtx *ctx, ASTNode *node);
            sem_symbolic_node_errnum(ctx, node);
        }
        else if (node->type == NODE_NAMESPACE) {
            sem_symbolic_namespace(ctx, node);
        }
        else if (node->type == NODE_COMPOUND) {
            CompoundNode *cn = (CompoundNode*)node;
            ASTNode *curr_body = cn->body;
            while (curr_body) {
                char *template_name = NULL;
                if (curr_body->type == NODE_FUNC_DEF) {
                    template_name = ((FuncDefNode*)curr_body)->name;
                } else if (curr_body->type == NODE_CLASS) {
                    template_name = ((ClassNode*)curr_body)->name;
                }
                if (template_name) {
                    VarType type_tmpl = {TYPE_UNKNOWN};
                    SemSymbol *sym = sem_symbol_add(ctx, template_name, SYM_TEMPLATE, type_tmpl);
                    sym->template_node = cn; // Should we create a new CompoundNode for each? Actually they all share the same cn, but wait...
                    // In instantiation, if it instantiates cn->body, it will instantiate EVERYTHING inside!
                    // This is actually what C++ does (for class templates with out-of-line methods, etc). Wait, if we instantiate the whole block, it will duplicate everything.
                }
                curr_body = curr_body->next;
            }
        }
        node = node->next;
    }
}

void sem_inject_default_class_args(SemanticCtx *ctx, CallNode *node, SemSymbol *sym, int arg_count, int total_fields) {
    VarDeclNode **fields = arena_alloc(ctx->compiler_ctx->arena, sizeof(VarDeclNode*) * total_fields);
    int idx = 0;
    sem_collect_class_fields(ctx, sym, fields, &idx);

    ASTNode *last_arg = node->args;
    if (last_arg) {
        while(last_arg->next) last_arg = last_arg->next;
    }

    for (int i = arg_count; i < total_fields; i++) {
        ASTNode *injected = NULL;
        if (fields[i] && fields[i]->initializer) {
            injected = ast_clone(ctx->compiler_ctx, fields[i]->initializer, NULL, NULL, 0, NULL, NULL, 0);
            injected->next = NULL; // important! ast_clone copies the next chain!
        }
        // If we don't have an initializer, maybe an error should have been thrown, but we'll try to inject something (like 0) or just let it crash later if required.
        // But since required_fields is checked, fields[i] WILL have an initializer here if valid.
        if (injected) {
            sem_check_expr(ctx, injected);
            if (last_arg) {
                last_arg->next = injected;
                last_arg = injected;
            } else {
                node->args = injected;
                last_arg = injected;
            }
        }
    }
}

void sem_check_binary_op(SemanticCtx *ctx, BinaryOpNode *node) {
    sem_check_expr(ctx, node->left);
    sem_check_expr(ctx, node->right);

    VarType l = sem_get_node_type(ctx, node->left);
    VarType r = sem_get_node_type(ctx, node->right);

    // Operator Overloading Check
    char name_buf[64];
    snprintf(name_buf, sizeof(name_buf), "__op_%d_%d", TOKEN_INFOP, node->op);
    SemSymbol *sym = sem_symbol_lookup(ctx, name_buf, NULL);
    if (sym && sym->kind == SYM_FUNC) {
        ASTNode *args = node->left;
        args->next = node->right;
        node->right->next = NULL;
        SemSymbol *resolved = sem_resolve_overload(ctx, &args, NULL, sym, NULL);
        if (resolved) {
            node->overloaded_func_name = arena_strdup(ctx->compiler_ctx->arena, resolved->mangled_name ? resolved->mangled_name : resolved->name);
            sem_set_node_type(ctx, (ASTNode*)node, resolved->type);
            node->left = args;
            node->right = args->next;
            node->left->next = NULL;
            if (node->right) node->right->next = NULL;
            return;
        }
        // Restore next pointers if not resolved
        node->left->next = NULL;
        node->right->next = NULL;
    }

    if (node->op == TOKEN_QUESTION || node->op == TOKEN_QUESTION_QUESTION) {
        if (node->fallback_err_name) {
            sem_set_node_tainted(ctx, (ASTNode*)node, sem_get_node_type(ctx, node->left).is_tainted);
        } else {
            sem_set_node_tainted(ctx, (ASTNode*)node, 0); // Result is pristine!
        }
    } else if (sem_get_node_type(ctx, node->left).is_tainted || sem_get_node_type(ctx, node->right).is_tainted) {
        sem_set_node_tainted(ctx, (ASTNode*)node, 1);
    } else if (node->op == TOKEN_SLASH) {
        if (!sem_is_constant_nonzero(node->right)) {
            sem_set_node_tainted(ctx, (ASTNode*)node, 1);
        }
    }

    if (l.base == TYPE_UNKNOWN || r.base == TYPE_UNKNOWN) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }

    if ((l.base == TYPE_VOID && l.ptr_depth == 0) || (r.base == TYPE_VOID && r.ptr_depth == 0)) {
        sem_error(ctx, (ASTNode*)node, "Operand of binary expression cannot be 'void'");
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }

    if (node->op == TOKEN_AND_AND || node->op == TOKEN_OR_OR) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_BOOL, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }

    if (node->op == TOKEN_QUESTION || node->op == TOKEN_QUESTION_QUESTION) {
        // Fallback uses the type of the left hand side, but preserve the taint state we just calculated
        int calculated_taint = sem_get_node_tainted(ctx, (ASTNode*)node) || sem_get_node_type(ctx, (ASTNode*)node).is_tainted;
        VarType res_ty = l;
        res_ty.is_tainted = calculated_taint;
        sem_set_node_type(ctx, (ASTNode*)node, res_ty);

        // Register the error ID if it's a specific fallback
        if (node->fallback_err_name) {
            void *err_val = hashmap_get(&ctx->compiler_ctx->error_table, node->fallback_err_name);
            if (!err_val && strncmp(node->fallback_err_name, "Err", 3) == 0) {
                int id = ctx->compiler_ctx->next_error_id++;
                hashmap_put(&ctx->compiler_ctx->error_table, node->fallback_err_name, (void*)(intptr_t)(id + 1));
            }
        }

        return;
    }

    // todo fix this casting!
    if (node->op == TOKEN_EQ || node->op == TOKEN_NEQ ||
        node->op == TOKEN_LT || node->op == TOKEN_GT ||
        node->op == TOKEN_LTE || node->op == TOKEN_GTE) {

        if (is_numeric(l) && is_numeric(r)) {
            VarType target_type = {TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
            if (l.base == TYPE_LONG_DOUBLE || r.base == TYPE_LONG_DOUBLE) target_type.base = TYPE_LONG_DOUBLE;
            else if (l.base == TYPE_DOUBLE || r.base == TYPE_DOUBLE) target_type.base = TYPE_DOUBLE;
            else if (l.base == TYPE_SINGLE || r.base == TYPE_SINGLE) target_type.base = TYPE_SINGLE;
            else if (l.base == TYPE_LONG || r.base == TYPE_LONG) target_type.base = TYPE_LONG;
            else if (l.base == TYPE_UNSIGNED_INT || r.base == TYPE_UNSIGNED_INT) target_type.base = TYPE_UNSIGNED_INT;
            if (l.is_tainted || r.is_tainted) target_type.is_tainted = 1;
            sem_insert_implicit_cast(ctx, &node->left, target_type);
            sem_insert_implicit_cast(ctx, &node->right, target_type);
        }

        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_BOOL, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }

    if (is_numeric(l) && is_numeric(r)) {
        VarType target_type = {TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};

        if (l.base == TYPE_LONG_DOUBLE || r.base == TYPE_LONG_DOUBLE) target_type.base = TYPE_LONG_DOUBLE;
        else if (l.base == TYPE_DOUBLE || r.base == TYPE_DOUBLE) target_type.base = TYPE_DOUBLE;
        else if (l.base == TYPE_SINGLE || r.base == TYPE_SINGLE) target_type.base = TYPE_SINGLE;
        else if (l.base == TYPE_LONG || r.base == TYPE_LONG) target_type.base = TYPE_LONG;
        else if (l.base == TYPE_UNSIGNED_INT || r.base == TYPE_UNSIGNED_INT) target_type.base = TYPE_UNSIGNED_INT;
        if (l.is_tainted || r.is_tainted) target_type.is_tainted = 1;

        sem_insert_implicit_cast(ctx, &node->left, target_type);
        sem_insert_implicit_cast(ctx, &node->right, target_type);

        sem_set_node_type(ctx, (ASTNode*)node, target_type);
    }
    else if (is_pointer(l) && is_integer(r)) {
         sem_set_node_type(ctx, (ASTNode*)node, l);
    }
    else if (is_integer(l) && is_pointer(r)) {
         sem_set_node_type(ctx, (ASTNode*)node, r);
    }
    else if ((l.base == TYPE_CLASS && l.class_name && streq_lit(l.class_name, "string")) || (r.base == TYPE_CLASS && r.class_name && streq_lit(r.class_name, "string"))) {
         if (node->op == TOKEN_PLUS)
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){ .base = TYPE_CLASS, .class_name = (char*)"string" });
         else {
            sem_error(ctx, (ASTNode*)node, "Invalid operation on strings");
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
         }
    }
    else {
        sem_error(ctx, (ASTNode*)node, "Invalid operands for binary operator");
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
    }
}

// TODO break this into a modularized form!
// because this is too big!
void sem_check_expr(SemanticCtx *ctx, ASTNode *node) {
    if (!node) return;
    if (node->filename) ctx->current_filename = node->filename;
    ctx->current_node = node;
    if (node->is_macro_arg) return;

    debug_semantic("sem_check_expr: type=%d line=%d col=%d node=%p\n", node->type, node->line, node->col, (void*)node);

    switch(node->type) {
        case NODE_LITERAL: {
            LiteralNode *lit = (LiteralNode*)node;
            sem_set_node_type(ctx, node, lit->var_type);
            break;
        }
        case NODE_ISCOMPATIBLE: {
            IsCompatibleNode *icn = (IsCompatibleNode*)node;
            VarType t1 = icn->target_type;
            VarType t2 = icn->target_type2;

            bool comp = sem_types_are_compatible(ctx, t2, t1);

            if (!comp && t1.base == TYPE_CLASS && t1.class_name) {
                char as_name[256];
                if (t2.base == TYPE_CLASS || t2.base == TYPE_UNKNOWN) {
                    snprintf(as_name, sizeof(as_name), "as_%s", t2.class_name ? t2.class_name : "");
                } else if (t2.base == TYPE_INT) {
                    snprintf(as_name, sizeof(as_name), "as_int");
                } else if (t2.base == TYPE_SINGLE) {
                    snprintf(as_name, sizeof(as_name), "as_float");
                } else {
                    snprintf(as_name, sizeof(as_name), "as_type%d", t2.base);
                }

                SemSymbol *class_sym = sem_symbol_lookup(ctx, t1.class_name, NULL);
                if (class_sym && class_sym->inner_scope) {
                    SemSymbol *member = class_sym->inner_scope->symbols;
                    while (member) {
                        if (member->kind == SYM_FUNC && streq_lit(member->name, as_name)) {
                            comp = true;
                            break;
                        }
                        member = member->next;
                    }
                }
            }

            node->type = NODE_LITERAL;
            LiteralNode *lit = (LiteralNode*)node;
            lit->var_type = (VarType){TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
            lit->val.long_val = comp ? 1 : 0;
            sem_set_node_type(ctx, node, lit->var_type);
            break;
        }
        case NODE_SIZEOF:
        case NODE_ALIGNOF: {
            SizeOfNode *sn = (SizeOfNode*)node;
            if (sn->target_type.base == TYPE_UNKNOWN && sn->operand) {
                sem_check_expr(ctx, sn->operand);
            } else if (sn->target_type.base == TYPE_CLASS && sn->target_type.ptr_depth == 0) {
                SemSymbol *sym = sem_symbol_lookup(ctx, sn->target_type.class_name, NULL);
                if (!sym || sym->kind != SYM_CLASS) {
                    sem_error(ctx, node, "Unknown class type '%s' in sizeof", sn->target_type.class_name);
                }
            }
            VarType size_type = {TYPE_UNSIGNED_LONG, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
            sem_set_node_type(ctx, node, size_type);
            break;
        }
        case NODE_VAR_REF: {
            sem_check_var_ref(ctx, node);
            break;
        }
        case NODE_BINARY_OP: sem_check_binary_op(ctx, (BinaryOpNode*)node); break;
        case NODE_UNARY_OP:
            sem_check_unary_op_switch(ctx, node);
            break;
        case NODE_CALL: sem_check_call(ctx, (CallNode*)node); break;
        case NODE_NAMED_ARG: {
            NamedArgNode *narg = (NamedArgNode*)node;
            sem_check_expr(ctx, narg->value);
            sem_set_node_type(ctx, node, sem_get_node_type(ctx, narg->value));
            if (sem_get_node_type(ctx, narg->value).is_tainted) sem_set_node_tainted(ctx, node, 1);
            break;
        }
        case NODE_MEMBER_ACCESS: sem_check_member_access(ctx, (MemberAccessNode*)node); break;
        case NODE_INDEX_ACCESS: {
            sem_check_index_access(ctx, node);
            break;
        }

        case NODE_BEING: {
            BeingNode *bn = (BeingNode*)node;
            sem_check_expr(ctx, bn->operand);
            if (sem_get_node_type(ctx, bn->operand).is_tainted) {
                sem_set_node_tainted(ctx, node, 1);
            }
            VarType op_t = sem_get_node_type(ctx, bn->operand);
            if (op_t.base == TYPE_VOID && op_t.ptr_depth == 0) {
                sem_error(ctx, node, "Cannot use \"being\" on \"void\" value");
            }
            sem_set_node_type(ctx, node, bn->var_type);
            break;
        }

        case NODE_CAST: {
            CastNode *cn = (CastNode*)node;
            cn->custom_cast_method = NULL;
            sem_check_expr(ctx, cn->operand);

            if (sem_get_node_type(ctx, cn->operand).is_tainted) {
                sem_set_node_tainted(ctx, node, 1);
            }

            VarType op_t = sem_get_node_type(ctx, cn->operand);
            if (op_t.base == TYPE_VOID && op_t.ptr_depth == 0) {
                sem_error(ctx, node, "Cannot cast 'void' value");
            }

            const char *op_class_name = op_t.class_name;
            if (!op_class_name && op_t.ptr_depth == 0 && op_t.array_size == 0) {
                if (op_t.base == TYPE_INT) op_class_name = "int";
                else if (op_t.base == TYPE_CHAR) op_class_name = "char";
                else if (op_t.base == TYPE_BOOL) op_class_name = "bool";
                else if (op_t.base == TYPE_SINGLE) op_class_name = "single";
                else if (op_t.base == TYPE_DOUBLE) op_class_name = "double";
            }

            if (op_class_name) {
                char as_name[256];
                if (cn->var_type.base == TYPE_CLASS || cn->var_type.base == TYPE_UNKNOWN) {
                    snprintf(as_name, sizeof(as_name), "as_%s", cn->var_type.class_name ? cn->var_type.class_name : "");
                } else if (cn->var_type.base == TYPE_INT) {
                    snprintf(as_name, sizeof(as_name), "as_int");
                } else if (cn->var_type.base == TYPE_SINGLE) {
                    snprintf(as_name, sizeof(as_name), "as_float");
                } else {
                    snprintf(as_name, sizeof(as_name), "as_type%d", cn->var_type.base);
                }

                SemSymbol *class_sym = sem_symbol_lookup(ctx, op_class_name, NULL);
                if (class_sym && class_sym->inner_scope) {
                    SemSymbol *member = class_sym->inner_scope->symbols;
                    while (member) {
                        if (member->kind == SYM_FUNC && streq_lit(member->name, as_name)) {
                            char mangled[512];
                            snprintf(mangled, sizeof(mangled), "%s_%s", op_class_name, as_name);
                            cn->custom_cast_method = arena_strdup(ctx->compiler_ctx->arena, member->mangled_name ? member->mangled_name : mangled);
                            break;
                        }
                        member = member->next;
                    }
                }
            }

            if (!cn->custom_cast_method) {
                if (op_t.base == TYPE_CLASS && op_t.class_name) {
                    SemSymbol *class_sym = sem_symbol_lookup(ctx, op_t.class_name, NULL);
                    if (class_sym && class_sym->is_union && class_sym->inner_scope) {
                        // First pass: exact match
                        SemSymbol *f = class_sym->inner_scope->symbols;
                        while (f) {
                            if (f->kind == SYM_VAR && sem_types_are_equal(f->type, cn->var_type)) {
                                MemberAccessNode ma;
                                memset(&ma, 0, sizeof(MemberAccessNode));
                                ma.base.type = NODE_MEMBER_ACCESS;
                                ma.base.line = node->line;
                                ma.base.col = node->col;
                                ma.object = cn->operand;
                                ma.member_name = arena_strdup(ctx->compiler_ctx->arena, f->name);

                                sem_set_node_type(ctx, node, f->type);
                                memcpy(node, &ma, sizeof(MemberAccessNode));
                                return;
                            }
                            f = f->next;
                        }
                        // Second pass: compatible match
                        f = class_sym->inner_scope->symbols;
                        while (f) {
                            if (f->kind == SYM_VAR && sem_types_are_compatible(ctx, f->type, cn->var_type)) {
                                MemberAccessNode ma;
                                memset(&ma, 0, sizeof(MemberAccessNode));
                                ma.base.type = NODE_MEMBER_ACCESS;
                                ma.base.line = node->line;
                                ma.base.col = node->col;
                                ma.object = cn->operand;
                                ma.member_name = arena_strdup(ctx->compiler_ctx->arena, f->name);

                                sem_set_node_type(ctx, node, f->type);
                                memcpy(node, &ma, sizeof(MemberAccessNode));
                                return;
                            }
                            f = f->next;
                        }
                    }
                }
                if (!sem_types_are_castable(ctx, cn->var_type, op_t)) {
                    char *t1 = sem_type_to_str(op_t);
                    char *t2 = sem_type_to_str(cn->var_type);
                    sem_error(ctx, node, "Cannot cast '%s' to '%s' (types are not compatible)", t1, t2);
                }
            }

            sem_set_node_type(ctx, node, cn->var_type);
            break;
        }
        case NODE_METHOD_CALL: {
            sem_check_method_call(ctx, (MethodCallNode*)node);
            break;
        }
        // Fix this shit
        case NODE_ARRAY_LIT: {
            ArrayLitNode *al = (ArrayLitNode*)node;
            ASTNode *el = al->elements;
            VarType elem_type = {TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
            int count = 0;

            if (el) {
                sem_check_expr(ctx, el);
                elem_type = sem_get_node_type(ctx, el);
                el = el->next;
                count++;
            }
            while(el) {
                sem_check_expr(ctx, el);
                VarType curr_type = sem_get_node_type(ctx, el);
                if (!sem_types_are_compatible(ctx, elem_type, curr_type)) {
                    sem_error(ctx, el, "Array elements have incompatible types");
                }
                el = el->next;
                count++;
            }
            if (elem_type.array_size > 0) {
                elem_type.array_depth = elem_type.array_size;
            }
            elem_type.array_size = count;
            sem_set_node_type(ctx, node, elem_type);
            break;
        }
        case NODE_INC_DEC: {
            IncDecNode *id = (IncDecNode*)node;
            sem_check_expr(ctx, id->target);
            VarType t = sem_get_node_type(ctx, id->target);

            // Operator Overloading Check
            char name_buf[64];
            snprintf(name_buf, sizeof(name_buf), "__op_%d_%d", id->is_prefix ? TOKEN_PREMUT : TOKEN_SUFMUT, id->op);
            SemSymbol *sym = NULL;
            int is_method = 0;
            if (t.base == TYPE_CLASS && t.ptr_depth == 0 && t.class_name) {
                SemSymbol *class_sym = sem_symbol_lookup(ctx, t.class_name, NULL);
                if (class_sym && class_sym->inner_scope) {
                    SemSymbol *s = class_sym->inner_scope->symbols;
                    while (s) {
                        if (streq_lit(s->name, name_buf)) { sym = s; is_method = 1; break; }
                        s = s->next;
                    }
                    if (!sym) {
                        snprintf(name_buf, sizeof(name_buf), "__op_%d_%d", id->is_prefix ? TOKEN_PREFOP : TOKEN_SUFFOP, id->op);
                        s = class_sym->inner_scope->symbols;
                        while (s) {
                            if (streq_lit(s->name, name_buf)) { sym = s; is_method = 1; break; }
                            s = s->next;
                        }
                    }
                }
            }
            if (!sym) {
                snprintf(name_buf, sizeof(name_buf), "__op_%d_%d", id->is_prefix ? TOKEN_PREMUT : TOKEN_SUFMUT, id->op);
                sym = sem_symbol_lookup(ctx, name_buf, NULL);
                if (!sym) {
                    snprintf(name_buf, sizeof(name_buf), "__op_%d_%d", id->is_prefix ? TOKEN_PREFOP : TOKEN_SUFFOP, id->op);
                    sym = sem_symbol_lookup(ctx, name_buf, NULL);
                }
            }
            if (sym && sym->kind == SYM_FUNC) {
                if (is_method) {
                    ASTNode *no_args = NULL;
                    SemSymbol *resolved = sem_resolve_overload(ctx, &no_args, NULL, sym, NULL);
                    if (resolved) {
                        id->overloaded_func_name = arena_strdup(ctx->compiler_ctx->arena, resolved->mangled_name ? resolved->mangled_name : resolved->name);
                        sem_set_node_type(ctx, (ASTNode*)node, resolved->type);
                        break;
                    }
                } else {
                    UnaryOpNode *addr_of = arena_alloc_type(ctx->compiler_ctx->arena, UnaryOpNode);
                    memset(addr_of, 0, sizeof(UnaryOpNode));
                    addr_of->base.type = NODE_UNARY_OP;
                    addr_of->op = TOKEN_AND;
                    addr_of->operand = id->target;
                    addr_of->base.line = node->line;
                    addr_of->base.col = node->col;
                    VarType ptr_type = t;
                    ptr_type.ptr_depth++;
                    sem_set_node_type(ctx, (ASTNode*)addr_of, ptr_type);

                    ASTNode *args = (ASTNode*)addr_of;
                    args->next = NULL;
                    SemSymbol *resolved = sem_resolve_overload(ctx, &args, NULL, sym, NULL);
                    if (resolved) {
                        id->overloaded_func_name = arena_strdup(ctx->compiler_ctx->arena, resolved->mangled_name ? resolved->mangled_name : resolved->name);
                        sem_set_node_type(ctx, (ASTNode*)node, resolved->type);
                        id->target = args;
                        id->target->next = NULL;
                        break;
                    }
                }
            }

            if (!is_numeric(t) && !is_pointer(t) && t.base != TYPE_UNKNOWN) {
                sem_error(ctx, node, "Cannot increment/decrement non-numeric/non-pointer type");
            }
            if (sem_get_node_type(ctx, id->target).is_tainted) sem_set_node_tainted(ctx, node, 1);
            sem_set_node_type(ctx, node, t);
            break;
        }
        case NODE_ASSIGN: {
            AssignNode *an = (AssignNode*)node;
            sem_check_assign(ctx, an);
            VarType t;
            if (an->name) {
                SemSymbol *sym = sem_symbol_lookup(ctx, an->name, NULL);
                t = sym ? sym->type : (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
            } else {
                t = sem_get_node_type(ctx, an->target);
            }
            sem_set_node_type(ctx, node, t);
            break;
        }

        case NODE_TYPEOF: {
            SizeOfNode *sn = (SizeOfNode*)node;
            if (sn->target_type.base == TYPE_UNKNOWN && sn->operand) {
                sem_check_expr(ctx, sn->operand);
            }
            sem_set_node_type(ctx, node, (VarType){ .base = TYPE_UNSIGNED_LONG, .ptr_depth = 0, .array_size = 0 });
            break;
        }
        case NODE_HAS_METHOD:
        case NODE_HAS_ATTRIBUTE: {
            UnaryOpNode *un = (UnaryOpNode*)node;

            // Do not check the operand because it might be a trait or class name which fails normally.
            // But wait, if it's a class name, it resolves to a type.
            // For now we will manually look it up.
            char *target_name = NULL;
            if (un->operand->type == NODE_VAR_REF) {
                target_name = ((VarRefNode*)un->operand)->name;
            } else if (un->operand->type == NODE_MEMBER_ACCESS) {
                MemberAccessNode *ma = (MemberAccessNode*)un->operand;
                sem_check_expr(ctx, ma->object);
                VarType obj_t = sem_get_node_type(ctx, ma->object);
                if (obj_t.base == TYPE_CLASS && obj_t.class_name) target_name = obj_t.class_name;
            }

            ASTNode *head = NULL;
            ASTNode **curr = &head;
            int count = 0;

            if (target_name) {
                SemSymbol *class_sym = sem_symbol_lookup(ctx, target_name, NULL);
                if (class_sym && class_sym->inner_scope) {
                    SemSymbol *s = class_sym->inner_scope->symbols;
                    while (s) {
                        bool match = false;
                        if (node->type == NODE_HAS_METHOD && s->kind == SYM_FUNC) match = true;
                        if (node->type == NODE_HAS_ATTRIBUTE && s->kind == SYM_VAR) match = true;

                        if (match) {
                            LiteralNode *ln = arena_alloc(ctx->compiler_ctx->arena, sizeof(LiteralNode));
                            ln->base.type = NODE_LITERAL;
                            if (ctx->compiler_ctx->settings.double_quote_as_string) {
                                ln->var_type.base = TYPE_CLASS;
                                ln->var_type.class_name = arena_strdup(ctx->compiler_ctx->arena, "string");
                                ln->var_type.ptr_depth = 0;
                            } else {
                                ln->var_type.base = TYPE_CHAR;
                                ln->var_type.class_name = NULL;
                                ln->var_type.ptr_depth = 1;
                            }
                            ln->var_type.array_size = 0;
                            ln->val.str_val = arena_strdup(ctx->compiler_ctx->arena, s->name);
                            sem_set_node_type(ctx, (ASTNode*)ln, ln->var_type);
                            *curr = (ASTNode*)ln;
                            curr = &(*curr)->next;
                            count++;
                        }
                        s = s->next;
                    }
                }
            }

            // Insert the length at index 0
            ASTNode *len_ast = NULL;
            if (ctx->compiler_ctx->settings.double_quote_as_string) {
                char length_str[16];
                snprintf(length_str, sizeof(length_str), "%d", count);
                LiteralNode *len_node = arena_alloc(ctx->compiler_ctx->arena, sizeof(LiteralNode));
                len_node->base.type = NODE_LITERAL;
                len_node->var_type.base = TYPE_CLASS;
                len_node->var_type.class_name = arena_strdup(ctx->compiler_ctx->arena, "string");
                len_node->var_type.ptr_depth = 0;
                len_node->var_type.array_size = 0;
                len_node->val.str_val = arena_strdup(ctx->compiler_ctx->arena, length_str);
                sem_set_node_type(ctx, (ASTNode*)len_node, len_node->var_type);
                len_ast = (ASTNode*)len_node;
            } else {
                LiteralNode *int_node = arena_alloc(ctx->compiler_ctx->arena, sizeof(LiteralNode));
                int_node->base.type = NODE_LITERAL;
                int_node->var_type.base = TYPE_INT;
                int_node->var_type.ptr_depth = 0;
                int_node->var_type.array_size = 0;
                int_node->val.int_val = count;

                CastNode *cast_node = arena_alloc(ctx->compiler_ctx->arena, sizeof(CastNode));
                cast_node->base.type = NODE_CAST;
                cast_node->var_type.base = TYPE_CHAR;
                cast_node->var_type.ptr_depth = 1;
                cast_node->var_type.array_size = 0;
                sem_set_node_type(ctx, (ASTNode*)int_node, int_node->var_type);
                cast_node->operand = (ASTNode*)int_node;
                sem_set_node_type(ctx, (ASTNode*)cast_node, cast_node->var_type);
                len_ast = (ASTNode*)cast_node;
            }

            // Terminate the symbol list
            *curr = NULL;

            len_ast->next = head;
            head = len_ast;
            count++;

            node->type = NODE_ARRAY_LIT;
            ArrayLitNode *an = (ArrayLitNode*)node;
            an->elements = head;

            VarType arr_t;
            if (ctx->compiler_ctx->settings.double_quote_as_string) {
                arr_t = (VarType){ .base = TYPE_CLASS, .class_name = arena_strdup(ctx->compiler_ctx->arena, "string"), .ptr_depth = 0, .array_size = count };
            } else {
                arr_t = (VarType){ .base = TYPE_CHAR, .class_name = NULL, .ptr_depth = 1, .array_size = count };
            }
            sem_set_node_type(ctx, node, arr_t);
            break;
        }
        case NODE_TEMPLATE_INSTANTIATION: {
            TemplateInstNode *ti = (TemplateInstNode*)node;
            if (ti->is_evaluated) {
                break;
            }
            char target_name[256] = "";
            SemSymbol *sym = NULL;
            SemScope *found_in_scope = NULL;

            if (ti->target->type == NODE_VAR_REF) {
                snprintf(target_name, sizeof(target_name), "%s", ((VarRefNode*)ti->target)->name);
                sym = sem_symbol_lookup(ctx, target_name, &found_in_scope);
            } else if (ti->target->type == NODE_MEMBER_ACCESS) {
                MemberAccessNode *ma = (MemberAccessNode*)ti->target;
                sem_check_expr(ctx, ma->object);
                VarType obj_type = sem_get_node_type(ctx, ma->object);

                if (obj_type.base == TYPE_CLASS && obj_type.class_name) {
                    SemSymbol *class_sym = sem_symbol_lookup_type(ctx, obj_type.class_name);
                    if (class_sym && class_sym->inner_scope && class_sym->inner_scope->symbol_map) {
                        sym = hashmap_get((HashMap*)class_sym->inner_scope->symbol_map, ma->member_name);
                        if (sym) {
                            found_in_scope = class_sym->inner_scope;
                            snprintf(target_name, sizeof(target_name), "%s", ma->member_name);
                        }
                    }
                } else if (obj_type.base == TYPE_NAMESPACE && obj_type.class_name) {
                    SemSymbol *ns_sym = sem_symbol_lookup(ctx, obj_type.class_name, NULL);
                    if (ns_sym && ns_sym->inner_scope && ns_sym->inner_scope->symbol_map) {
                        sym = hashmap_get((HashMap*)ns_sym->inner_scope->symbol_map, ma->member_name);
                        if (sym) {
                            found_in_scope = ns_sym->inner_scope;
                            snprintf(target_name, sizeof(target_name), "%s", ma->member_name);
                        }
                    }
                }

                // Fallback for simple namespace variable A.B if not typed correctly
                if (!sym && ma->object->type == NODE_VAR_REF) {
                    snprintf(target_name, sizeof(target_name), "%s.%s", ((VarRefNode*)ma->object)->name, ma->member_name);
                    sym = sem_symbol_lookup(ctx, target_name, &found_in_scope);
                }
            } else {
                sem_error(ctx, node, "Expected identifier for template instantiation");
                break;
            }

            if (!sym || sym->kind != SYM_TEMPLATE) {
                // Fallback to array access / component access!
                if (ti->num_template_types == 1) {
                    LiteralNode *index_ln = arena_alloc(ctx->compiler_ctx->arena, sizeof(LiteralNode));
                    index_ln->base.type = NODE_LITERAL;
                    index_ln->var_type = ti->template_types[0];
                    index_ln->val.long_val = ti->template_types[0].base;
                    index_ln->base.line = node->line;
                    index_ln->base.col = node->col;

                    IndexAccessNode *aa = (IndexAccessNode*)node;
                    aa->base.type = NODE_INDEX_ACCESS;
                    // aa->target is already ti->target, so we don't need to change it
                    aa->target = ti->target;
                    aa->index = (ASTNode*)index_ln;

                    sem_check_expr(ctx, node); // Check again as array access
                    break;
                }

                sem_error(ctx, node, "'%s' is not a template", target_name);
                break;
            }
            CompoundNode *cn = sym->template_node;
            if (ti->num_template_types != cn->num_type_params) {
                sem_error(ctx, node, "Template '%s' expects %d types, got %d", target_name, cn->num_type_params, ti->num_template_types);
                break;
            }

            for (int i = 0; i < ti->num_template_types; i++) {
                if (cn->num_allowed && cn->num_allowed[i] > 0) {
                    int match = 0;
                    for (int j = 0; j < cn->num_allowed[i]; j++) {
                        int is_compat = sem_types_are_equal(ti->template_types[i], cn->allowed_types[i][j]);
                        if (is_compat) {
                            match = 1;
                            break;
                        }
                    }
                    if (!match) {
                        char *t1 = sem_type_to_str(ti->template_types[i]);
                        sem_error(ctx, node, "Type '%s' is not allowed for template parameter '%s'", t1, cn->type_params[i]);
                    }
                }
            }

            // Generate mangled name
            char mangled[1024];
            snprintf(mangled, sizeof(mangled), "%s", target_name);
            for (int i = 0; i < ti->num_template_types; i++) {
                char *t_str = sem_type_to_str(ti->template_types[i]);
                strncat(mangled, "_", sizeof(mangled) - strlen(mangled) - 1);
                strncat(mangled, t_str, sizeof(mangled) - strlen(mangled) - 1);
            }

            SemSymbol *inst_sym = sem_symbol_lookup(ctx, mangled, NULL);
            if (!inst_sym) {
            // 1. Collect all top-level names in the block and their mangled names
            int num_renames = 0;
            char *rename_from[32];
            char *rename_to[32];
            ASTNode *cn_curr = cn->body;
            while (cn_curr) {
                char *base_name = NULL;
                if (cn_curr->type == NODE_FUNC_DEF) base_name = ((FuncDefNode*)cn_curr)->name;
                else if (cn_curr->type == NODE_CLASS) base_name = ((ClassNode*)cn_curr)->name;

                if (base_name) {
                    if (num_renames >= 32) {
                        sem_error(ctx, node, "Template has too many top-level symbols to instantiate");
                        break;
                    }
                    rename_from[num_renames] = base_name;
                    char node_mangled[1024];
                    snprintf(node_mangled, sizeof(node_mangled), "%s", base_name);
                    for (int i = 0; i < ti->num_template_types; i++) {
                        char *t_str = sem_type_to_str(ti->template_types[i]);
                        size_t nm_len = strlen(node_mangled);
                        if (nm_len + 1 < sizeof(node_mangled)) {
                            snprintf(node_mangled + nm_len, sizeof(node_mangled) - nm_len, "_%s", t_str);
                        }
                    }
                    rename_to[num_renames] = arena_strdup(ctx->compiler_ctx->arena, node_mangled);
                    num_renames++;
                }
                cn_curr = cn_curr->next;
            }

            for (int i = 0; i < num_renames; i++) {
                // printf("Rename: %s -> %s\n", rename_from[i], rename_to[i]);
            }

            // 2. Clone the body with replacements AND renames
            ASTNode *cloned_body = ast_clone(ctx->compiler_ctx, cn->body, cn->type_params, ti->template_types, ti->num_template_types, rename_from, rename_to, num_renames);

                debug_semantic("&ctx->ast_tail=%p, ctx->ast_tail=%p, *ctx->ast_tail=%p\n", &ctx->ast_tail, ctx->ast_tail, ctx->ast_tail ? *ctx->ast_tail : NULL);
                if (ctx->ast_tail) {
                    *ctx->ast_tail = cloned_body;
                    while (*ctx->ast_tail) {
                        ctx->ast_tail = &(*ctx->ast_tail)->next;
                    }
                }

                // Add to global AST? Just scan and check it now!
                SemScope *old_scope = ctx->current_scope;
                ctx->current_scope = found_in_scope ? found_in_scope : ctx->global_scope;
                sem_scan_top_level(ctx, cloned_body);
                // Also check it now, but for all nodes in the cloned body!
                ASTNode *curr = cloned_body;
                while (curr) {
                    sem_check_node(ctx, curr);
                    curr = curr->next;
                }
                ctx->current_scope = old_scope;
                if (found_in_scope) {
                    if (found_in_scope->symbol_map) {
                        inst_sym = hashmap_get((HashMap*)found_in_scope->symbol_map, mangled);
                    }
                    if (!inst_sym) {
                        SemSymbol *s = found_in_scope->symbols;
                        while (s) {
                            if (streq_lit(s->name, mangled)) {
                                inst_sym = s;
                                break;
                            }
                            s = s->next;
                        }
                    }
                } else {
                    inst_sym = sem_symbol_lookup(ctx, mangled, NULL);
                }
            }

            // Replace the current node with a VarRef to the mangled name, so codegen just calls the instantiated function/class
            // Wait, this is an expression! A template instantiation `map[int]` resolves to the function name itself.
            // So its type should be the type of `inst_sym`.
            if (inst_sym) {
                sem_set_node_type(ctx, node, inst_sym->type);
                if (ti->target->type == NODE_MEMBER_ACCESS) {
                    MemberAccessNode *ma = (MemberAccessNode*)ti->target;
                    MemberAccessNode *new_ma = arena_alloc(ctx->compiler_ctx->arena, sizeof(MemberAccessNode));
                    new_ma->base.type = NODE_MEMBER_ACCESS;
                    new_ma->object = ma->object;
                    new_ma->member_name = arena_strdup(ctx->compiler_ctx->arena, mangled);
                    new_ma->base.line = node->line;
                    new_ma->base.col = node->col;
                    ti->target = (ASTNode*)new_ma;
                } else {
                    VarRefNode *new_vr = arena_alloc(ctx->compiler_ctx->arena, sizeof(VarRefNode));
                    new_vr->base.type = NODE_VAR_REF;
                    new_vr->name = arena_strdup(ctx->compiler_ctx->arena, mangled);
                    new_vr->base.line = node->line;
                    new_vr->base.col = node->col;
                    ti->target = (ASTNode*)new_vr;
                }
            }
            ti->is_evaluated = 1;
            break;
        }
          case NODE_IMPORT_EXPR: {
               ImportExprNode *ie = (ImportExprNode*)node;
               const char *import_path = ie->path;
               if (!import_path) {
                   import_path = ctx->compiler_ctx->current_namespace;
                   if (!import_path || !import_path[0]) {
                       sem_error(ctx, node, "import() requires a string literal path");
                       sem_set_node_type(ctx, node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
                       break;
                   }
                   VarType ns_type = {TYPE_NAMESPACE, 0, arena_strdup(ctx->compiler_ctx->arena, import_path), 0, 0, NULL, NULL, 0, 0, 0, 0};
                   sem_set_node_type(ctx, node, ns_type);
                   break;
               }
               if (ie->header == HEADER_C) {
                   SemSymbol *ns_sym = sem_symbol_lookup(ctx, import_path, NULL);
                   if (ns_sym && ns_sym->kind == SYM_NAMESPACE) {
                       VarType ns_type = {TYPE_NAMESPACE, 0, arena_strdup(ctx->compiler_ctx->arena, ns_sym->name), 0, 0, NULL, NULL, 0, 0, 0, 0};
                       sem_set_node_type(ctx, node, ns_type);
                   } else {
                       if (ie->resolved_body) {
                           VarType ns_type = {TYPE_NAMESPACE, 0, arena_strdup(ctx->compiler_ctx->arena, import_path), 0, 0, NULL, NULL, 0, 0, 0, 0};
                           ns_sym = sem_symbol_add(ctx, import_path, SYM_NAMESPACE, ns_type);
                           
                           SemScope *ns_scope = arena_alloc_type(ctx->compiler_ctx->arena, SemScope);
                           memset(ns_scope, 0, sizeof(SemScope));
                           ns_scope->symbols = NULL;
                           ns_scope->symbol_map = arena_alloc_type(ctx->compiler_ctx->arena, HashMap);
                           hashmap_init((HashMap*)ns_scope->symbol_map, ctx->compiler_ctx->arena, 16);
                           ns_scope->parent = ctx->current_scope;
                           ns_scope->is_function_scope = 0;
                           ns_scope->is_class_scope = 0;
                           ns_sym->inner_scope = ns_scope;
                           
                           SemScope *old_scope = ctx->current_scope;
                           ctx->current_scope = ns_scope;
                           sem_scan_top_level(ctx, ie->resolved_body);
                           ASTNode *curr = ie->resolved_body;
                           while (curr) { sem_check_node(ctx, curr); curr = curr->next; }
                           ctx->current_scope = old_scope;
                           
                           sem_set_node_type(ctx, node, ns_type);
                       } else {
                           sem_error(ctx, node, "Could not resolve C header: '%s'", import_path);
                           sem_set_node_type(ctx, node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
                       }
                   }
               } else {
                   SemSymbol *ns_sym = sem_symbol_lookup(ctx, import_path, NULL);
                    if (!ns_sym || ns_sym->kind != SYM_NAMESPACE) {
                        if (ie->resolved_body) {
                            sem_scan_top_level(ctx, ie->resolved_body);
                            ASTNode *curr = ie->resolved_body;
                            while (curr) { sem_check_node(ctx, curr); curr = curr->next; }
                        }
                        ns_sym = sem_symbol_lookup(ctx, import_path, NULL);
                    }
                   if (!ns_sym || ns_sym->kind != SYM_NAMESPACE) {
                       const char *last_slash = strrchr(import_path, '/');
                       if (last_slash) {
                           ns_sym = sem_symbol_lookup(ctx, last_slash + 1, NULL);
                       }
                   }
                   if (!ns_sym || ns_sym->kind != SYM_NAMESPACE) {
                       const char *last_slash = strrchr(import_path, '/');
                       if (last_slash) {
                           char first[256];
                           size_t len = (size_t)(last_slash - import_path);
                           if (len >= sizeof(first)) len = sizeof(first) - 1;
                           memcpy(first, import_path, len);
                           first[len] = '\0';
                           ns_sym = sem_symbol_lookup(ctx, first, NULL);
                       }
                   }
                   if (ns_sym && ns_sym->kind == SYM_NAMESPACE) {
                       VarType ns_type = {TYPE_NAMESPACE, 0, arena_strdup(ctx->compiler_ctx->arena, ns_sym->name), 0, 0, NULL, NULL, 0, 0, 0, 0};
                       sem_set_node_type(ctx, node, ns_type);
                   } else {
                       sem_error(ctx, node, "import('%s'): namespace not found", import_path);
                       sem_set_node_type(ctx, node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
                   }
               }
               break;
           }
        default: break;
    }
}

