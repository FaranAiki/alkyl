SemSymbol* sem_resolve_overload(SemanticCtx *ctx, ASTNode **args, int *out_arg_count, SemSymbol *first_sym, ASTNode *err_node) {
    int arg_count = 0;
    ASTNode *curr_arg = *args;
    while(curr_arg) {
        sem_check_expr(ctx, curr_arg);
        curr_arg = curr_arg->next;
        arg_count++;
    }
    if (out_arg_count) *out_arg_count = arg_count;

    SemSymbol *sym = first_sym;
    SemSymbol *best_match = NULL;
    
    // Find matching overload (exact types or compatible implicit cast)
    while (sym) {
        if (sym->param_count == arg_count || sym->is_variadic) {
            int match = 1;
            curr_arg = *args;
            Parameter *curr_para = sym->params;
            while(curr_arg && curr_para) {
                if (!sem_types_are_compatible(ctx, curr_para->type, sem_get_node_type(ctx, curr_arg))) {
                    match = 0;
                    break;
                }
                curr_arg = curr_arg->next;
                curr_para = curr_para->next;
            }
            if (match) {
                best_match = sym;
                break;
            }
        }
        sym = sym->overload_next;
    }
    
    if (!best_match) {
        sem_error(ctx, err_node, "No matching overload found for function '%s'", first_sym->name);
        return NULL;
    }
    
    // Apply implicit casts
    ASTNode **p_curr = args;
    Parameter *curr_para = best_match->params;
    while(*p_curr && curr_para) {
        if (sem_types_are_compatible(ctx, curr_para->type, sem_get_node_type(ctx, *p_curr))) {
            sem_insert_implicit_cast(ctx, p_curr, curr_para->type);
        }
        p_curr = &(*p_curr)->next;
        curr_para = curr_para->next;
    }
    
    return best_match;
}
