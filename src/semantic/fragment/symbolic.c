#include "symbolic.h"

void sem_symbolic_func_def(SemanticCtx *ctx, ASTNode *node) {
    FuncDefNode *fd = (FuncDefNode*)node;
    SemSymbol *sym = sem_symbol_add(ctx, fd->name, SYM_FUNC, fd->ret_type);
    sym->is_is_a = fd->is_is_a;
    sym->is_has_a = fd->is_has_a;
    sym->is_pure = !fd->is_extern;
    sym->must_pure = fd->is_pure;
    sym->is_pristine = !fd->is_extern;
    sym->must_pristine = fd->is_pristine;
    sym->is_flux = fd->is_flux;
    sym->is_variadic = fd->is_varargs; 
    sym->params = fd->params; // idk if this is redundant or not but ok
    Parameter *p = fd->params;
    while (p) {
        sym->param_count++;
        p = p->next;
    }
}

void sem_symbolic_var_decl(SemanticCtx *ctx, ASTNode *node) {
    VarDeclNode *vd = (VarDeclNode*)node;
    SemSymbol *sym = sem_symbol_add(ctx, vd->name, SYM_VAR, vd->var_type);
    sym->is_mutable = vd->is_mutable;
    sym->is_pure = 1;
    sym->must_pure = vd->is_pure;
    sym->is_pristine = 1;
    sym->must_pristine = vd->is_pristine;
}

void sem_symbolic_node_enum(SemanticCtx *ctx, ASTNode *node) {
    EnumNode *en = (EnumNode*)node;
    
    VarType enum_type = {TYPE_ENUM, 0, 0, arena_strdup(ctx->compiler_ctx->arena, en->name), 0, NULL, NULL, 0, 0, 0, 0};
    SemSymbol *sym = sem_symbol_add(ctx, en->name, SYM_ENUM, enum_type);
    
    SemScope *enum_scope = arena_alloc_type(ctx->compiler_ctx->arena, SemScope);
    memset(enum_scope, 0, sizeof(SemScope));

    enum_scope->symbols = NULL;
    enum_scope->parent = ctx->current_scope;
    enum_scope->is_function_scope = 0;
    enum_scope->is_class_scope = 0; 
    sym->inner_scope = enum_scope;
    
    EnumEntry *entry = en->entries;
    while(entry) {
        SemSymbol *mem = arena_alloc_type(ctx->compiler_ctx->arena, SemSymbol);
        memset(mem, 0, sizeof(SemSymbol));

        mem->name = arena_strdup(ctx->compiler_ctx->arena, entry->name);
        mem->kind = SYM_VAR; 
        mem->type = enum_type; 
        mem->is_mutable = 0;
        mem->is_initialized = 1;
        mem->is_pure = 0; // todo default to false
        mem->is_pristine = 0; // todo default to false
        mem->params = NULL;
        mem->param_count = 0;
        mem->parent_name = NULL;
        mem->inner_scope = NULL;
        
        mem->next = enum_scope->symbols;
        enum_scope->symbols = mem;
        
        entry = entry->next;
    }
}

void sem_symbolic_namespace(SemanticCtx *ctx, ASTNode *node) {
    NamespaceNode *ns = (NamespaceNode*)node;
    VarType ns_type = {TYPE_NAMESPACE, 0, 0, arena_strdup(ctx->compiler_ctx->arena, ns->name), 0, NULL, NULL, 0, 0, 0, 0};
    SemSymbol *sym = sem_symbol_add(ctx, ns->name, SYM_NAMESPACE, ns_type);
    
    SemScope *ns_scope = arena_alloc_type(ctx->compiler_ctx->arena, SemScope);
    memset(ns_scope, 0, sizeof(SemScope));

    ns_scope->symbols = NULL;
    ns_scope->parent = ctx->current_scope;
    ns_scope->is_function_scope = 0;
    ns_scope->is_class_scope = 0;
    sym->inner_scope = ns_scope;
    
    SemScope *old = ctx->current_scope;
    ctx->current_scope = ns_scope;
    sem_scan_top_level(ctx, ns->body);
    ctx->current_scope = old;
}
