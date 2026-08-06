#include "class.h"

void sem_check_member_access(SemanticCtx *ctx, MemberAccessNode *node) {
    sem_check_expr(ctx, node->object);
    VarType obj_type = sem_get_node_type(ctx, node->object);
    
    if (sem_get_node_tainted(ctx, node->object)) {
        sem_set_node_tainted(ctx, (ASTNode*)node, 1);
    }
    
    if (obj_type.base == TYPE_UNKNOWN) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }
    
    const char *primitive_class_name = NULL;
    if (obj_type.base != TYPE_CLASS && obj_type.ptr_depth == 0) {
        if (obj_type.base == TYPE_INT) primitive_class_name = "int";
        else if (obj_type.base == TYPE_CHAR) primitive_class_name = "char";
        else if (obj_type.base == TYPE_BOOL) primitive_class_name = "bool";
        else if (obj_type.base == TYPE_SINGLE) primitive_class_name = "single";
        else if (obj_type.base == TYPE_DOUBLE) primitive_class_name = "double";
    }

    if ((obj_type.base == TYPE_CLASS && obj_type.class_name) || primitive_class_name) {
        const char *lookup_name = primitive_class_name ? primitive_class_name : obj_type.class_name;
        SemSymbol *class_sym = sem_symbol_lookup_type(ctx, lookup_name);
        if (!class_sym || class_sym->kind != SYM_CLASS) {
            if (class_sym) { debug_semantic("'%s' kind is %d in class.c\n", lookup_name, class_sym->kind); }
            if (class_sym && class_sym->kind == SYM_TEMPLATE) {
                CompoundNode *cn = class_sym->template_node;
                char expected_types[256] = "";
                size_t pos = 0;
                for (int i=0; i<cn->num_type_params; i++) {
                    pos += snprintf(expected_types + pos, sizeof(expected_types) - pos, "%s", cn->type_params[i]);
                    if (i < cn->num_type_params - 1 && pos < sizeof(expected_types) - 1) {
                        pos += snprintf(expected_types + pos, sizeof(expected_types) - pos, ", ");
                    }
                }
                sem_error(ctx, (ASTNode*)node, "'%s' needs types [%s]", lookup_name, expected_types);
            } else {
                sem_error(ctx, (ASTNode*)node, "Type '%s' is not a class/struct", lookup_name);
            }
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
            return;
        }
        
        SemSymbol *current_class = class_sym;
        int found = 0;
        
        while (current_class) {
            if (current_class->inner_scope && current_class->inner_scope->symbol_map) {
                SemSymbol *member = hashmap_get((HashMap*)current_class->inner_scope->symbol_map, node->member_name);
                if (member) {
                    sem_set_node_type(ctx, (ASTNode*)node, member->type);
                    found = 1;
                    if (sem_get_node_tainted(ctx, node->object) && !member->is_pristine) {
                        sem_set_node_tainted(ctx, (ASTNode*)node, 1);
                    } else if (member->is_pristine) {
                        sem_set_node_tainted(ctx, (ASTNode*)node, 0);
                    }
                    goto done_search;
                }
            }
            if (current_class->trait_count > 0) {
                for (int i = 0; i < current_class->trait_count; i++) {
                    SemSymbol *trait_sym = sem_symbol_lookup(ctx, current_class->traits[i], NULL);
                    if (trait_sym && trait_sym->inner_scope && trait_sym->inner_scope->symbol_map) {
                        SemSymbol *member = hashmap_get((HashMap*)trait_sym->inner_scope->symbol_map, node->member_name);
                        if (member) {
                            sem_set_node_type(ctx, (ASTNode*)node, member->type);
                            found = 1;
                            char *obj_name = "obj";
                            int should_warn = 1;
                            if (node->object) {
                                if (node->object->type == NODE_VAR_REF) {
                                    obj_name = ((VarRefNode*)node->object)->name;
                                } else if (node->object->type == NODE_INDEX_ACCESS) {
                                    IndexAccessNode *aa = (IndexAccessNode*)node->object;
                                    if (aa->index->type == NODE_VAR_REF) {
                                        VarRefNode *vr = (VarRefNode*)aa->index;
                                        if (streq(vr->name, trait_sym->name)) {
                                            should_warn = 0;
                                        }
                                    }
                                }
                            }
                            if (should_warn) {
                                sem_warning(ctx, (ASTNode*)node, "%s is from %s, consider %s[%s]", node->member_name, trait_sym->name, obj_name, trait_sym->name);
                            }
                            goto done_search;
                        }
                    }
                }
            }
            if (current_class->parent_name) {
                current_class = sem_symbol_lookup(ctx, current_class->parent_name, NULL);
            } else {
                current_class = NULL;
            }
        }
        
        done_search:
        if (!found) {
            sem_error(ctx, (ASTNode*)node, "Class '%s' has no member named '%s'", obj_type.class_name, node->member_name);
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
        }
    }
    else if (obj_type.base == TYPE_ENUM && obj_type.class_name) {
        SemSymbol *enum_sym = sem_symbol_lookup(ctx, obj_type.class_name, NULL);
        
        if (!enum_sym || enum_sym->kind != SYM_ENUM) {
            sem_error(ctx, (ASTNode*)node, "'%s' is not an enum", obj_type.class_name);
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
            return;
        }

        if (enum_sym->inner_scope) {
             SemSymbol *member = enum_sym->inner_scope->symbols;
             while (member) {
                 if (streq(member->name, node->member_name)) {
                     sem_set_node_type(ctx, (ASTNode*)node, member->type);
                     return;
                 }
                 member = member->next;
             }
        }
        sem_error(ctx, (ASTNode*)node, "Enum '%s' has no member '%s'", obj_type.class_name, node->member_name);
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
    }
    else if (obj_type.base == TYPE_NAMESPACE && obj_type.class_name) {
        SemSymbol *ns_sym = sem_symbol_lookup(ctx, obj_type.class_name, NULL);
        if (!ns_sym || ns_sym->kind != SYM_NAMESPACE) {
            sem_error(ctx, (ASTNode*)node, "'%s' is not a namespace", obj_type.class_name);
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
            return;
        }

        if (ns_sym->inner_scope) {
             SemSymbol *member = ns_sym->inner_scope->symbols;
             while (member) {
                 if (streq(member->name, node->member_name)) {
                     sem_set_node_type(ctx, (ASTNode*)node, member->type);
                     return;
                 }
                 member = member->next;
             }
        }
        sem_error(ctx, (ASTNode*)node, "Namespace '%s' has no member '%s'", obj_type.class_name, node->member_name);
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
    }
    else if (obj_type.base == TYPE_CLASS && obj_type.class_name && streq(obj_type.class_name, "string") && streq(node->member_name, "length")) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
    }
    else {
        sem_error(ctx, (ASTNode*)node, "Cannot access member on non-class/non-enum/non-namespace type");
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0});
    }
}

void sem_scan_class_members(SemanticCtx *ctx, ClassNode *cn, SemSymbol *class_sym) {
    if (!ctx->compiler_ctx || !ctx->compiler_ctx->arena) return;

    SemScope *class_scope = NULL;
    if (cn->is_extended && class_sym->inner_scope) {
        class_scope = class_sym->inner_scope;
    } else {
        class_scope = arena_alloc_type(ctx->compiler_ctx->arena, SemScope);
        memset(class_scope, 0, sizeof(SemScope));
        class_scope->symbols = NULL;
        class_scope->symbol_map = arena_alloc_type(ctx->compiler_ctx->arena, HashMap);
        hashmap_init((HashMap*)class_scope->symbol_map, ctx->compiler_ctx->arena, 16);
        class_scope->parent = ctx->current_scope; 
        class_scope->is_function_scope = 0;
        class_scope->is_class_scope = 1; 
        class_scope->class_sym = class_sym; 
        class_scope->expected_ret_type = (VarType){0};
        
        class_sym->inner_scope = class_scope;
    }
    
    SemScope *old_scope = ctx->current_scope;
    ctx->current_scope = class_scope;
    
    ASTNode *mem = cn->members;
    // DO this is why we should separate the shit out of this
    while(mem) {
        if (mem->type == NODE_VAR_DECL) {
            sem_symbolic_var_decl(ctx, mem);
        } else if (mem->type == NODE_FUNC_DEF) {
            FuncDefNode *fd = (FuncDefNode*)mem;
            if (cn->is_extended) {
                SemSymbol *existing = hashmap_get((HashMap*)class_scope->symbol_map, fd->name);
                if (existing) {
                    if (!fd->is_override) {
                        sem_error(ctx, mem, "method '%s' already exists in class '%s', use override to force it", fd->name, class_sym->name);
                    } else {
                        sem_warning(ctx, mem, "overriding method '%s' in extended class", fd->name);
                    }
                }
            }
            sem_symbolic_func_def(ctx, mem);
        } else if (mem->type == NODE_COMPOUND) {
            CompoundNode *cn = (CompoundNode*)mem;
            if (cn->body->type == NODE_FUNC_DEF) {
                FuncDefNode *f = (FuncDefNode*)cn->body;
                VarType type_tmpl = {TYPE_UNKNOWN};
                SemSymbol *sym = sem_symbol_add(ctx, f->name, SYM_TEMPLATE, type_tmpl);
                sym->template_node = cn;
            }
        }
        mem = mem->next;
    }
    
    ctx->current_scope = old_scope;
    
    SemSymbol *s = class_sym->inner_scope->symbols;
    while(s) {
        s = s->next;
    }
}

