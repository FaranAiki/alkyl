        ASTNode *init = NULL;
        if (p->current_token.type == TOKEN_ASSIGN) {
            eat(p, TOKEN_ASSIGN);
            init = parse_expression(p);
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
            cnode->name = current_vtype.class_name ? parser_strdup(p, current_vtype.class_name) : parser_strdup(p, "unknown_type");
            cnode->target = NULL;
            cnode->args = args_head;
            init = (ASTNode*)cnode;
        } else {
            if (current_vtype.base == TYPE_AUTO) {
                parser_fail(p, "'let' variable declaration must have an initializer");
            }
        }
