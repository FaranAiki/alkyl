ASTNode* parse_initializer(Parser *p, VarType vtype) {
    if (p->current_token.type == TOKEN_ASSIGN) {
        eat(p, TOKEN_ASSIGN);
        return parse_expression(p);
    } else if (p->current_token.type == TOKEN_LPAREN) {
        eat(p, TOKEN_LPAREN);
        ASTNode *args_head = NULL;
        ASTNode **curr_arg = &args_head;
        if (p->current_token.type != TOKEN_RPAREN) {
            while (1) {
                ASTNode *expr = parse_expression(p);
                if (expr->type == NODE_ASSIGN && ((AssignNode*)expr)->op == TOKEN_ASSIGN && ((AssignNode*)expr)->name != NULL) {
                    NamedArgNode *narg = parser_alloc(p, sizeof(NamedArgNode));
                    narg->base.type = NODE_NAMED_ARG;
                    narg->name = ((AssignNode*)expr)->name;
                    narg->value = ((AssignNode*)expr)->value;
                    narg->base.line = expr->line; narg->base.col = expr->col;
                    expr = (ASTNode*)narg;
                }
                *curr_arg = expr;
                curr_arg = &(*curr_arg)->next;
                if (p->current_token.type == TOKEN_COMMA) {
                    eat(p, TOKEN_COMMA);
                } else {
                    break;
                }
            }
        }
        eat(p, TOKEN_RPAREN);
        CallNode *cnode = parser_alloc(p, sizeof(CallNode));
        cnode->base.type = NODE_CALL;
        cnode->name = vtype.class_name ? parser_strdup(p, vtype.class_name) : NULL;
        cnode->target = NULL;
        cnode->args = args_head;
        return (ASTNode*)cnode;
    }
    return NULL;
}
