#include "parser_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void set_loc(ASTNode *n, int line, int col) {
    if(n) { n->line = line; n->col = col; }
}

ASTNode* parse_return(Lexer *l) {
  int line = current_token.line, col = current_token.col;
  eat(l, TOKEN_RETURN);
  ASTNode *val = NULL;
  if (current_token.type != TOKEN_SEMICOLON) {
    val = parse_expression(l);
  }
  eat(l, TOKEN_SEMICOLON);
  ReturnNode *node = calloc(1, sizeof(ReturnNode));
  node->base.type = NODE_RETURN;
  node->value = val;
  set_loc((ASTNode*)node, line, col);
  return (ASTNode*)node;
}

ASTNode* parse_break(Lexer *l) {
    int line = current_token.line, col = current_token.col;
    eat(l, TOKEN_BREAK);
    eat(l, TOKEN_SEMICOLON);
    BreakNode *node = calloc(1, sizeof(BreakNode));
    node->base.type = NODE_BREAK;
    set_loc((ASTNode*)node, line, col);
    return (ASTNode*)node;
}

ASTNode* parse_continue(Lexer *l) {
    int line = current_token.line, col = current_token.col;
    eat(l, TOKEN_CONTINUE);
    eat(l, TOKEN_SEMICOLON);
    ContinueNode *node = calloc(1, sizeof(ContinueNode));
    node->base.type = NODE_CONTINUE;
    set_loc((ASTNode*)node, line, col);
    return (ASTNode*)node;
}

ASTNode* parse_assignment_or_call(Lexer *l) {
  // Capture start token for location and error reporting
  Token start_token = current_token;
  if (start_token.text) start_token.text = strdup(start_token.text); 

  int line = current_token.line;
  int col = current_token.col;

  // 1. Parse Identifier
  char *name = current_token.text;
  current_token.text = NULL; 
  eat(l, TOKEN_IDENTIFIER);
  
  ASTNode *node = calloc(1, sizeof(VarRefNode));
  ((VarRefNode*)node)->base.type = NODE_VAR_REF;
  ((VarRefNode*)node)->name = name;
  set_loc(node, line, col);

  // 2. Check for Standard Call (start with parens)
  if (current_token.type == TOKEN_LPAREN) {
      char *fname = ((VarRefNode*)node)->name;
      free(node); 
      node = parse_call(l, fname);
      set_loc(node, line, col); // Ensure location is preserved
  }

  // 3. Apply Postfix Operations
  node = parse_postfix(l, node);

  // 4. Check for Assignment
  int is_assign = 0;
  switch (current_token.type) {
      case TOKEN_ASSIGN:
      case TOKEN_PLUS_ASSIGN:
      case TOKEN_MINUS_ASSIGN:
      case TOKEN_STAR_ASSIGN:
      case TOKEN_SLASH_ASSIGN:
      case TOKEN_MOD_ASSIGN:
      case TOKEN_AND_ASSIGN:
      case TOKEN_OR_ASSIGN:
      case TOKEN_XOR_ASSIGN:
      case TOKEN_LSHIFT_ASSIGN:
      case TOKEN_RSHIFT_ASSIGN:
          is_assign = 1;
          break;
      default:
          is_assign = 0;
  }

  if (is_assign) {
    int op = current_token.type;
    eat(l, op); 
    ASTNode *expr = parse_expression(l);
    eat(l, TOKEN_SEMICOLON);

    AssignNode *an = calloc(1, sizeof(AssignNode));
    an->base.type = NODE_ASSIGN;
    an->value = expr;
    an->op = op;
    
    if (node->type == NODE_VAR_REF) {
        an->name = ((VarRefNode*)node)->name;
        ((VarRefNode*)node)->name = NULL; free(node);
    } else {
        an->target = node; 
    }
    set_loc((ASTNode*)an, line, col);
    if (start_token.text) free(start_token.text);
    return (ASTNode*)an;
  }
  
  // 5. Check for Paren-less Call
  if (node->type == NODE_VAR_REF) {
      TokenType t = current_token.type;
      int is_arg_start = (t == TOKEN_NUMBER || t == TOKEN_FLOAT || t == TOKEN_STRING || 
            t == TOKEN_CHAR_LIT || t == TOKEN_TRUE || t == TOKEN_FALSE || 
            t == TOKEN_IDENTIFIER || t == TOKEN_LPAREN || t == TOKEN_LBRACKET || 
            t == TOKEN_NOT || t == TOKEN_BIT_NOT || t == TOKEN_MINUS || t == TOKEN_PLUS || t == TOKEN_STAR || t == TOKEN_AND || t == TOKEN_TYPEOF);

      if (is_arg_start) {
          char *fname = ((VarRefNode*)node)->name;
          free(node); 
          
          ASTNode *args_head = NULL;
          ASTNode **curr_arg = &args_head;
          
          *curr_arg = parse_expression(l);
          curr_arg = &(*curr_arg)->next;

          while (current_token.type == TOKEN_COMMA) {
              eat(l, TOKEN_COMMA);
              *curr_arg = parse_expression(l);
              curr_arg = &(*curr_arg)->next;
          }
          eat(l, TOKEN_SEMICOLON);

          CallNode *cn = calloc(1, sizeof(CallNode));
          cn->base.type = NODE_CALL;
          cn->name = fname;
          cn->args = args_head;
          set_loc((ASTNode*)cn, line, col);
          if (start_token.text) free(start_token.text);
          return (ASTNode*)cn;
      }
  }

  // 6. Statement End
  if (current_token.type == TOKEN_SEMICOLON) {
      eat(l, TOKEN_SEMICOLON);
      if (start_token.text) free(start_token.text);
      return node; 
  }
  
  // --- Enhanced Error Reporting (Cleaned up) ---
  
  char msg[256];
  snprintf(msg, sizeof(msg), "Invalid statement starting with identifier '%s'.", 
           ((VarRefNode*)node)->name);
  
  if (node->type == NODE_VAR_REF) {
      if (((VarRefNode*)node)->name) free(((VarRefNode*)node)->name);
  }
  free(node);
  
  parser_fail_at(l, start_token, msg);
  
  if (start_token.text) free(start_token.text);
  return NULL;
}

ASTNode* parse_var_decl_internal(Lexer *l) {
  int line = current_token.line, col = current_token.col;
  int is_mut = 1; 
  if (current_token.type == TOKEN_KW_MUT) { is_mut = 1; eat(l, TOKEN_KW_MUT); }
  else if (current_token.type == TOKEN_KW_IMUT) { is_mut = 0; eat(l, TOKEN_KW_IMUT); }
  
  VarType vtype = parse_type(l);

  if (current_token.type == TOKEN_KW_MUT) { is_mut = 1; eat(l, TOKEN_KW_MUT); }
  else if (current_token.type == TOKEN_KW_IMUT) { is_mut = 0; eat(l, TOKEN_KW_IMUT); }

  if (current_token.type != TOKEN_IDENTIFIER) { 
      parser_fail(l, "Expected variable name after type in declaration"); 
  }
  char *name = current_token.text;
  current_token.text = NULL;
  eat(l, TOKEN_IDENTIFIER);
  
  int is_array = 0;
  ASTNode *array_size = NULL;
  
  if (current_token.type == TOKEN_LBRACKET) {
    is_array = 1;
    eat(l, TOKEN_LBRACKET);
    if (current_token.type != TOKEN_RBRACKET) {
      array_size = parse_expression(l);
    }
    eat(l, TOKEN_RBRACKET);
  }

  ASTNode *init = NULL;
  if (current_token.type == TOKEN_ASSIGN) {
    eat(l, TOKEN_ASSIGN);
    init = parse_expression(l);
  }

  eat(l, TOKEN_SEMICOLON);
  
  VarDeclNode *node = calloc(1, sizeof(VarDeclNode));
  node->base.type = NODE_VAR_DECL;
  node->var_type = vtype;
  node->name = name;
  node->initializer = init;
  node->is_mutable = is_mut;
  node->is_array = is_array;
  node->array_size = array_size;
  set_loc((ASTNode*)node, line, col);
  return (ASTNode*)node;
}

ASTNode* parse_single_statement_or_block(Lexer *l) {
  if (current_token.type == TOKEN_LBRACE) {
    eat(l, TOKEN_LBRACE);
    ASTNode *block = parse_statements(l);
    eat(l, TOKEN_RBRACE);
    return block;
  }
  
  int line = current_token.line, col = current_token.col;

  if (current_token.type == TOKEN_LOOP) return parse_loop(l);
  if (current_token.type == TOKEN_WHILE) return parse_while(l);
  if (current_token.type == TOKEN_IF) return parse_if(l);
  if (current_token.type == TOKEN_RETURN) return parse_return(l);
  if (current_token.type == TOKEN_BREAK) return parse_break(l);
  if (current_token.type == TOKEN_CONTINUE) return parse_continue(l);
  
  VarType peek_t = parse_type(l); 
  if (peek_t.base != TYPE_UNKNOWN) {
      if (peek_t.base == TYPE_CLASS && current_token.type == TOKEN_LPAREN) {
          ASTNode* call = parse_call(l, peek_t.class_name);
          eat(l, TOKEN_SEMICOLON);
          set_loc(call, line, col);
          return call;
      }

      int is_mut = 1;
      if (current_token.type == TOKEN_KW_MUT) { is_mut = 1; eat(l, TOKEN_KW_MUT); }
      else if (current_token.type == TOKEN_KW_IMUT) { is_mut = 0; eat(l, TOKEN_KW_IMUT); }
      
      if (current_token.type != TOKEN_IDENTIFIER) parser_fail(l, "Expected variable name in declaration");
      char *name = current_token.text;
      current_token.text = NULL;
      eat(l, TOKEN_IDENTIFIER);
      
      int is_array = 0;
      ASTNode *array_size = NULL;
      if (current_token.type == TOKEN_LBRACKET) {
        is_array = 1;
        eat(l, TOKEN_LBRACKET);
        if (current_token.type != TOKEN_RBRACKET) array_size = parse_expression(l);
        eat(l, TOKEN_RBRACKET);
      }
      ASTNode *init = NULL;
      if (current_token.type == TOKEN_ASSIGN) {
        eat(l, TOKEN_ASSIGN);
        init = parse_expression(l);
      }
      eat(l, TOKEN_SEMICOLON);
      VarDeclNode *node = calloc(1, sizeof(VarDeclNode));
      node->base.type = NODE_VAR_DECL;
      node->var_type = peek_t;
      node->name = name;
      node->initializer = init;
      node->is_mutable = is_mut;
      node->is_array = is_array;
      node->array_size = array_size;
      set_loc((ASTNode*)node, line, col);
      return (ASTNode*)node;
  }
  
  if (current_token.type == TOKEN_KW_MUT || current_token.type == TOKEN_KW_IMUT) {
      return parse_var_decl_internal(l);
  }

  if (current_token.type == TOKEN_IDENTIFIER) return parse_assignment_or_call(l);
  
  ASTNode *expr = parse_expression(l);
  if (current_token.type == TOKEN_SEMICOLON) eat(l, TOKEN_SEMICOLON);
  return expr;
}

ASTNode* parse_loop(Lexer *l) {
  int line = current_token.line, col = current_token.col;
  eat(l, TOKEN_LOOP);
  ASTNode *expr = parse_expression(l);
  LoopNode *node = calloc(1, sizeof(LoopNode));
  node->base.type = NODE_LOOP;
  node->iterations = expr;
  node->body = parse_single_statement_or_block(l);
  set_loc((ASTNode*)node, line, col);
  return (ASTNode*)node;
}

ASTNode* parse_while(Lexer *l) {
    int line = current_token.line, col = current_token.col;
    eat(l, TOKEN_WHILE);
    int is_do_while = 0;
    if (current_token.type == TOKEN_ONCE) {
        eat(l, TOKEN_ONCE);
        is_do_while = 1;
    }
    ASTNode *cond = parse_expression(l);
    ASTNode *body = parse_single_statement_or_block(l);
    WhileNode *node = calloc(1, sizeof(WhileNode));
    node->base.type = NODE_WHILE;
    node->condition = cond;
    node->body = body;
    node->is_do_while = is_do_while;
    set_loc((ASTNode*)node, line, col);
    return (ASTNode*)node;
}

ASTNode* parse_if(Lexer *l) {
  int line = current_token.line, col = current_token.col;
  eat(l, TOKEN_IF);
  ASTNode *cond = parse_expression(l);
  ASTNode *then_body = parse_single_statement_or_block(l);
  ASTNode *else_body = NULL;
  if (current_token.type == TOKEN_ELIF) {
    current_token.type = TOKEN_IF; 
    else_body = parse_if(l);
  } else if (current_token.type == TOKEN_ELSE) {
    eat(l, TOKEN_ELSE);
    else_body = parse_single_statement_or_block(l);
  }
  IfNode *node = calloc(1, sizeof(IfNode));
  node->base.type = NODE_IF;
  node->condition = cond;
  node->then_body = then_body;
  node->else_body = else_body;
  set_loc((ASTNode*)node, line, col);
  return (ASTNode*)node;
}

ASTNode* parse_statements(Lexer *l) {
  ASTNode *head = NULL;
  ASTNode **current = &head;
  while (current_token.type != TOKEN_EOF && current_token.type != TOKEN_RBRACE) {
    ASTNode *stmt = parse_single_statement_or_block(l);
    if (stmt) {
      *current = stmt;
      current = &stmt->next;
    }
  }
  return head;
}
