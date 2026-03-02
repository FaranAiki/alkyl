#include "class.h"

void sem_check_member_access(SemanticCtx *ctx, MemberAccessNode *node) {
    sem_check_expr(ctx, node->object);
    VarType obj_type = sem_get_node_type(ctx, node->object);
    
    if (sem_get_node_tainted(ctx, node->object)) {
        sem_set_node_tainted(ctx, (ASTNode*)node, 1);
    }
    
    if (obj_type.base == TYPE_UNKNOWN) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, NULL, NULL, 0, 0, 0, 0});
        return;
    }
    
    if (obj_type.base == TYPE_CLASS && obj_type.class_name) {
        SemSymbol *class_sym = sem_symbol_lookup(ctx, obj_type.class_name, NULL);
        if (!class_sym || class_sym->kind != SYM_CLASS) {
            sem_error(ctx, (ASTNode*)node, "Type '%s' is not a class/struct", obj_type.class_name);
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, NULL, NULL, 0, 0, 0, 0});
            return;
        }
        
        SemSymbol *current_class = class_sym;
        int found = 0;
        
        while (current_class) {
            if (current_class->inner_scope) {
                SemSymbol *member = current_class->inner_scope->symbols;
                while (member) {
                    if (strcmp(member->name, node->member_name) == 0) {
                        sem_set_node_type(ctx, (ASTNode*)node, member->type);
                        found = 1;
                        goto done_search;
                    }
                    member = member->next;
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
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, NULL, NULL, 0, 0, 0, 0});
        }
    }
    else if (obj_type.base == TYPE_ENUM && obj_type.class_name) {
        SemSymbol *enum_sym = sem_symbol_lookup(ctx, obj_type.class_name, NULL);
        
        if (!enum_sym || enum_sym->kind != SYM_ENUM) {
            sem_error(ctx, (ASTNode*)node, "'%s' is not an enum", obj_type.class_name);
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, NULL, NULL, 0, 0, 0, 0});
            return;
        }

        if (enum_sym->inner_scope) {
             SemSymbol *member = enum_sym->inner_scope->symbols;
             while (member) {
                 if (strcmp(member->name, node->member_name) == 0) {
                     sem_set_node_type(ctx, (ASTNode*)node, member->type);
                     return;
                 }
                 member = member->next;
             }
        }
        sem_error(ctx, (ASTNode*)node, "Enum '%s' has no member '%s'", obj_type.class_name, node->member_name);
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, NULL, NULL, 0, 0, 0, 0});
    }
    else if (obj_type.base == TYPE_NAMESPACE && obj_type.class_name) {
        SemSymbol *ns_sym = sem_symbol_lookup(ctx, obj_type.class_name, NULL);
        if (!ns_sym || ns_sym->kind != SYM_NAMESPACE) {
            sem_error(ctx, (ASTNode*)node, "'%s' is not a namespace", obj_type.class_name);
            sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, NULL, NULL, 0, 0, 0, 0});
            return;
        }

        if (ns_sym->inner_scope) {
             SemSymbol *member = ns_sym->inner_scope->symbols;
             while (member) {
                 if (strcmp(member->name, node->member_name) == 0) {
                     sem_set_node_type(ctx, (ASTNode*)node, member->type);
                     return;
                 }
                 member = member->next;
             }
        }
        sem_error(ctx, (ASTNode*)node, "Namespace '%s' has no member '%s'", obj_type.class_name, node->member_name);
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, NULL, NULL, 0, 0, 0, 0});
    }
    else if (obj_type.base == TYPE_STRING && strcmp(node->member_name, "length") == 0) {
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_INT, 0, 0, NULL, 0, NULL, NULL, 0, 0, 0, 0});
    }
    else {
        sem_error(ctx, (ASTNode*)node, "Cannot access member on non-class/non-enum/non-namespace type");
        sem_set_node_type(ctx, (ASTNode*)node, (VarType){TYPE_UNKNOWN, 0, 0, NULL, 0, NULL, NULL, 0, 0, 0, 0});
    }
}

void sem_scan_class_members(SemanticCtx *ctx, ClassNode *cn, SemSymbol *class_sym) {
    if (!ctx->compiler_ctx || !ctx->compiler_ctx->arena) return;

    SemScope *class_scope = arena_alloc_type(ctx->compiler_ctx->arena, SemScope);
    memset(class_scope, 0, sizeof(SemScope));

    class_scope->symbols = NULL;
    class_scope->parent = ctx->current_scope; 
    class_scope->is_function_scope = 0;
    class_scope->is_class_scope = 1; 
    class_scope->class_sym = class_sym; 
    class_scope->expected_ret_type = (VarType){0};
    
    class_sym->inner_scope = class_scope;
    
    SemScope *old_scope = ctx->current_scope;
    ctx->current_scope = class_scope;
    
    ASTNode *mem = cn->members;
    while(mem) {
        if (mem->type == NODE_VAR_DECL) {
            VarDeclNode *vd = (VarDeclNode*)mem;
            SemSymbol *sym = sem_symbol_add(ctx, vd->name, SYM_VAR, vd->var_type);
            sym->is_mutable = vd->is_mutable;
            sym->is_pure = true;
            sym->must_pure = vd->is_pure;
            sym->is_pristine = true;
            sym->must_pristine = vd->is_pristine;
        } else if (mem->type == NODE_FUNC_DEF) {
            FuncDefNode *fd = (FuncDefNode*)mem;
            SemSymbol *sym = sem_symbol_add(ctx, fd->name, SYM_FUNC, fd->ret_type);
            sym->is_is_a = fd->is_is_a;
            sym->is_has_a = fd->is_has_a;
            sym->is_pure = !fd->is_extern;
            sym->must_pure = fd->is_pure;
            sym->is_pristine = !fd->is_extern;
            sym->must_pristine = fd->is_pristine;
            sym->is_flux = fd->is_flux; // Attach coroutine context
        }
        mem = mem->next;
    }
    
    ctx->current_scope = old_scope;
}

