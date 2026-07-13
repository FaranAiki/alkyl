#include "parser_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

ASTNode* parse_while(Parser *p) {
    int line = p->current_token.line, col = p->current_token.col;
    eat(p, TOKEN_WHILE);
    int is_do_while = 0;
    if (p->current_token.type == TOKEN_ONCE) {
        eat(p, TOKEN_ONCE);
        is_do_while = 1;
    }
    ASTNode *cond = NULL;
    if (p->settings.require_parens_for_conditions) {
        eat(p, TOKEN_LPAREN);
        cond = parse_expression(p);
        eat(p, TOKEN_RPAREN);
    } else {
        cond = parse_expression(p);
    }
    ASTNode *body = parse_single_statement_or_block(p);
    WhileNode *node = parser_alloc(p, sizeof(WhileNode));
    node->base.type = NODE_WHILE;
    node->condition = cond;
    node->body = body;
    node->is_do_while = is_do_while;
    set_loc((ASTNode*)node, line, col);
    return (ASTNode*)node;
}

ASTNode* parse_loop(Parser *p) {
  int line = p->current_token.line, col = p->current_token.col;
  eat(p, TOKEN_LOOP);
  ASTNode *expr = parse_expression(p);
  LoopNode *node = parser_alloc(p, sizeof(LoopNode));
  node->base.type = NODE_LOOP;
  node->iterations = expr;
  node->body = parse_single_statement_or_block(p);
  set_loc((ASTNode*)node, line, col);
  return (ASTNode*)node;
}

ASTNode* parse_for_in(Parser *p) {
    int line = p->current_token.line, col = p->current_token.col;
    eat(p, TOKEN_FOR);
    
    if (p->settings.require_parens_for_conditions) eat(p, TOKEN_LPAREN);
    
    if (p->current_token.type != TOKEN_IDENTIFIER) parser_fail(p, "Expected identifier after 'for'");
    char *var_name = parser_strdup(p, p->current_token.text);
    eat(p, TOKEN_IDENTIFIER);
    
    if (p->current_token.type != TOKEN_IN) parser_fail(p, "Expected 'in' after variable in for-loop");
    eat(p, TOKEN_IN);
    
    ASTNode *collection = parse_expression(p);
    
    if (p->settings.require_parens_for_conditions) eat(p, TOKEN_RPAREN);
    ASTNode *body = parse_single_statement_or_block(p);
    
    ForInNode *node = parser_alloc(p, sizeof(ForInNode));
    node->base.type = NODE_FOR_IN;
    node->var_name = var_name;
    node->collection = collection;
    node->body = body;
    set_loc((ASTNode*)node, line, col);
    return (ASTNode*)node;
}

ASTNode* parse_break(Parser *p) {
    int line = p->current_token.line, col = p->current_token.col;
    eat(p, TOKEN_BREAK);
    eat_semi(p);
    BreakNode *node = parser_alloc(p, sizeof(BreakNode));
    node->base.type = NODE_BREAK;
    set_loc((ASTNode*)node, line, col);
    return (ASTNode*)node;
}

ASTNode* parse_continue(Parser *p) {
    int line = p->current_token.line, col = p->current_token.col;
    eat(p, TOKEN_CONTINUE);
    eat_semi(p);
    ContinueNode *node = parser_alloc(p, sizeof(ContinueNode));
    node->base.type = NODE_CONTINUE;
    set_loc((ASTNode*)node, line, col);
    return (ASTNode*)node;
}