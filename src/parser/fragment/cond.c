#include "parser_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static ASTNode* parse_case_body_stmts(Parser *p);
ASTNode* parse_if(Parser *p) {
  int line = p->current_token.line, col = p->current_token.col;
  eat(p, TOKEN_IF);
  ASTNode *cond = NULL;
  if (p->settings.require_parens_for_conditions) {
      eat(p, TOKEN_LPAREN);
      cond = parse_expression(p);
      eat(p, TOKEN_RPAREN);
  } else {
      cond = parse_expression(p);
  }
  
  if (p->current_token.type == TOKEN_THEN) {
      eat(p, TOKEN_THEN);
  }
  
  ASTNode *then_body = parse_single_statement_or_block(p);
  ASTNode *else_body = NULL;
  if (p->current_token.type == TOKEN_ELIF) {
    p->current_token.type = TOKEN_IF; 
    else_body = parse_if(p);
  } else if (p->current_token.type == TOKEN_ELSE) {
    eat(p, TOKEN_ELSE);
    else_body = parse_single_statement_or_block(p);
  }
  IfNode *node = parser_alloc(p, sizeof(IfNode));
  node->base.type = NODE_IF;
  node->condition = cond;
  node->then_body = then_body;
  node->else_body = else_body;
  set_loc((ASTNode*)node, line, col);
  return (ASTNode*)node;
}

ASTNode* parse_switch(Parser *p) {
    int line = p->current_token.line, col = p->current_token.col;
    eat(p, TOKEN_SWITCH);
    
    ASTNode *cond = NULL;
    if (p->settings.require_parens_for_conditions) {
        eat(p, TOKEN_LPAREN);
        cond = parse_expression(p);
        eat(p, TOKEN_RPAREN);
    } else {
        cond = parse_expression(p);
    }
    
    eat(p, TOKEN_LBRACE);

    ASTNode *cases_head = NULL;
    ASTNode **cases_curr = &cases_head;
    ASTNode *default_body = NULL;

    while (p->current_token.type != TOKEN_RBRACE && p->current_token.type != TOKEN_EOF) {
        int is_leak = 0;
        int case_line = p->current_token.line;
        int case_col = p->current_token.col;

        if (p->current_token.type == TOKEN_LEAK) {
            eat(p, TOKEN_LEAK);
            is_leak = 1;
        }
        
        if (p->current_token.type == TOKEN_CASE) {
            eat(p, TOKEN_CASE);
            
            while(1) {
                ASTNode *val = parse_expression(p);
                
                if (p->current_token.type == TOKEN_COMMA) {
                    eat(p, TOKEN_COMMA);
                    CaseNode *cn = parser_alloc(p, sizeof(CaseNode));
                    cn->base.type = NODE_CASE;
                    cn->value = val;
                    cn->body = NULL; 
                    cn->is_leak = 1; 
                    set_loc((ASTNode*)cn, case_line, case_col);
                    
                    *cases_curr = (ASTNode*)cn;
                    cases_curr = &cn->base.next;
                } else {
                    eat(p, TOKEN_COLON);
                    ASTNode *body = parse_case_body_stmts(p);
                    
                    CaseNode *cn = parser_alloc(p, sizeof(CaseNode));
                    cn->base.type = NODE_CASE;
                    cn->value = val;
                    cn->body = body;
                    cn->is_leak = is_leak; 
                    set_loc((ASTNode*)cn, case_line, case_col);
                    
                    *cases_curr = (ASTNode*)cn;
                    cases_curr = &cn->base.next;
                    break; 
                }
            }
        } 
        else if (p->current_token.type == TOKEN_DEFAULT) {
             eat(p, TOKEN_DEFAULT);
             eat(p, TOKEN_COLON);
             if (default_body) parser_fail(p, "Duplicate default case");
             default_body = parse_case_body_stmts(p);
        } else {
            parser_fail(p, "Expected 'case', 'leak case', or 'default' inside switch");
        }
    }
    eat(p, TOKEN_RBRACE);

    SwitchNode *node = parser_alloc(p, sizeof(SwitchNode));
    node->base.type = NODE_SWITCH;
    node->condition = cond;
    node->cases = cases_head;
    node->default_case = default_body;
    set_loc((ASTNode*)node, line, col);
    return (ASTNode*)node;
}

static ASTNode* parse_case_body_stmts(Parser *p) {
    ASTNode *head = NULL;
    ASTNode **current = &head;
    while (p->current_token.type != TOKEN_EOF && 
           p->current_token.type != TOKEN_RBRACE && 
           p->current_token.type != TOKEN_CASE && 
           p->current_token.type != TOKEN_LEAK &&
           p->current_token.type != TOKEN_DEFAULT) {
        ASTNode *stmt = parse_single_statement_or_block(p);
        if (stmt) {
            *current = stmt;
            current = &stmt->next;
        }
    }
    // *current = NULL; // don't have to cut off
    return head;
}