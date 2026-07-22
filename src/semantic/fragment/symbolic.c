#include "symbolic.h"

void sem_symbolic_func_def(SemanticCtx *ctx, ASTNode *node) {
    FuncDefNode *fd = (FuncDefNode*)node;
    SemSymbol *sym = sem_symbol_add(ctx, fd->name, SYM_FUNC, fd->ret_type);
    sym->is_is_a = fd->is_is_a;
    sym->is_has_a = fd->is_has_a;
    sym->is_pure = fd->is_pure && !fd->is_extern;
    sym->must_pure = fd->has_explicit_pure;
    sym->is_pristine = fd->is_pristine;
    sym->must_pristine = fd->has_explicit_pristine;
    sym->has_explicit_pristine = fd->has_explicit_pristine;
    sym->is_flux = fd->is_flux;
    sym->is_macro = fd->is_macro;
    sym->node_ptr = node;
    sym->is_variadic = fd->is_varargs; 
    sym->params = fd->params; // idk if this is redundant or not but ok
    Parameter *p = fd->params;
    while (p) {
        if (fd->is_extern && !p->has_explicit_pristine) p->is_pristine = 0;
        sym->param_count++;
        p = p->next;
    }
    
    if (fd->is_extern) {
        sym->is_pristine = 1;
        sym->type.is_tainted = 0;
    }

    char *mangled = fd->name;
    if (fd->is_extern) {
        if (fd->extern_name) mangled = fd->extern_name;
    } else if (strcmp(fd->name, "main") != 0) {
        mangled = sem_mangle_func_name(ctx, fd->class_name, fd->name, fd->params);
    }
    sym->mangled_name = mangled;
    fd->mangled_name = mangled;

    if (fd->has_errnum) {
        sym->has_errnum = 1;
        sym->num_err = fd->num_err;
        sym->err_names = fd->err_names;
    }
}

void sem_symbolic_var_decl(SemanticCtx *ctx, ASTNode *node) {
    VarDeclNode *vd = (VarDeclNode*)node;
    SemSymbol *sym = sem_symbol_add(ctx, vd->name, SYM_VAR, vd->var_type);
    sym->is_mutable = vd->is_mutable;
    sym->is_pure = vd->is_pure;
    sym->must_pure = vd->has_explicit_pure;
    sym->is_pristine = vd->is_pristine;
    sym->must_pristine = vd->has_explicit_pristine;
}

void sem_symbolic_node_enum(SemanticCtx *ctx, ASTNode *node) {
    EnumNode *en = (EnumNode*)node;
    
    VarType enum_type = {TYPE_ENUM, 0, arena_strdup(ctx->compiler_ctx->arena, en->name), 0, 0, NULL, NULL, 0, 0, 0, 0};
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
        mem->is_pure = 0; // impure by default
        mem->is_pristine = 1; // pristine (not tainted) by default
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
    VarType ns_type = {TYPE_NAMESPACE, 0, arena_strdup(ctx->compiler_ctx->arena, ns->name), 0, 0, NULL, NULL, 0, 0, 0, 0};
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
    
    const char *old_ns = arena_strdup(ctx->compiler_ctx->arena, diag_get_namespace(ctx->compiler_ctx));
    diag_set_namespace(ctx->compiler_ctx, ns->name);
    
    sem_scan_top_level(ctx, ns->body);
    
    diag_set_namespace(ctx->compiler_ctx, old_ns);
    ctx->current_scope = old;
}

void sem_symbolic_node_errnum(SemanticCtx *ctx, ASTNode *node) {
    ErrNumNode *en = (ErrNumNode*)node;
    EnumEntry *entry = en->entries;
    
    // Add them to the global error table.
    // If not found, assign next_error_id.
    while (entry) {
        if (!hashmap_get(&ctx->compiler_ctx->error_table, entry->name)) {
            // Next error id is next_error_id + 1. (0 is NoError)
            int id = ctx->compiler_ctx->next_error_id++;
            hashmap_put(&ctx->compiler_ctx->error_table, strdup(entry->name), (void*)(intptr_t)(id + 1));
            
            // Wait, also need to add them as variables so they can be referenced in code (like `purge ErrSomething;`)
            // The Purge check expects them to just be parsed as VarRef, but during type checking of other stuff,
            // they might be unresolved symbols. 
            // We can just add them to the global scope as integer constants.
            VarType err_type = {TYPE_INT, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0};
            SemSymbol *sym = sem_symbol_add(ctx, entry->name, SYM_VAR, err_type);
            sym->is_initialized = 1;
            sym->is_mutable = 0;
        }
        entry = entry->next;
    }
}
