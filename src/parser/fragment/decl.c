#include "parser_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

ASTNode* parse_var_decl_internal(Parser *p) {
  int line = p->current_token.line, col = p->current_token.col;
  int is_mut = 1; 
  if (p->current_token.type == TOKEN_KW_MUT) { is_mut = 1; eat(p, TOKEN_KW_MUT); }
  else if (p->current_token.type == TOKEN_KW_IMUT) { is_mut = 0; eat(p, TOKEN_KW_IMUT); }
  
  VarType vtype = parse_type(p);

  if (p->current_token.type == TOKEN_LPAREN) {
      char *name = NULL;
      vtype = parse_func_ptr_decl(p, vtype, &name);
      
      ASTNode *init = NULL;
      if (p->current_token.type == TOKEN_ASSIGN) {
          eat(p, TOKEN_ASSIGN);
          init = parse_expression(p);
      }
      eat_semi(p);
      
      VarDeclNode *node = parser_alloc(p, sizeof(VarDeclNode));
      node->base.type = NODE_VAR_DECL;
      node->var_type = vtype;
      node->name = name;
      node->initializer = init;
      node->is_mutable = is_mut;
      set_loc((ASTNode*)node, line, col);
      return (ASTNode*)node;
  }

  if (p->current_token.type == TOKEN_KW_MUT) { is_mut = 1; eat(p, TOKEN_KW_MUT); }
  else if (p->current_token.type == TOKEN_KW_IMUT) { is_mut = 0; eat(p, TOKEN_KW_IMUT); }

  if (p->current_token.type != TOKEN_IDENTIFIER) { 
      parser_fail(p, "Expected variable name after type in declaration"); 
  }
  char *name = p->current_token.text;
  p->current_token.text = NULL;
  eat(p, TOKEN_IDENTIFIER);
  
  if (p->settings.allow_postfix_types && p->current_token.type == TOKEN_COLON) {
      eat(p, TOKEN_COLON);
      VarType pt = parse_type(p);
      if (pt.base != TYPE_UNKNOWN) {
          vtype = pt;
      }
  }
  
  ASTNode *head = NULL;
  ASTNode **curr = &head;
  char *name_val = name;
  
  int next_extra_ptrs = 0;

  while(1) {
      VarType current_vtype = vtype;
      current_vtype.ptr_depth += next_extra_ptrs;
      
      if (p->current_token.type == TOKEN_QUESTION) {
          current_vtype.is_tainted = 1;
          eat(p, TOKEN_QUESTION);
      }
      
      int is_array = 0;
      ASTNode *array_size = NULL;
      ASTNode **curr_sz = &array_size;
      
      while (p->current_token.type == TOKEN_LBRACKET) { if (p->has_error) break;
        is_array = 1;
        current_vtype.ptr_depth++; 
        eat(p, TOKEN_LBRACKET);
        ASTNode *sz = NULL;
        if (p->current_token.type != TOKEN_RBRACKET) sz = parse_expression(p);
        else {
            LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
            ln->base.type = NODE_LITERAL;
            ln->var_type.base = TYPE_INT;
            ln->val.int_val = 0;
            sz = (ASTNode*)ln;
        }
        *curr_sz = sz;
        curr_sz = &sz->next;
        eat(p, TOKEN_RBRACKET);
      }

      ASTNode *init = parse_initializer(p, current_vtype);
      if (!init && current_vtype.base == TYPE_AUTO) {
          parser_fail(p, "'let' variable declaration must have an initializer");
      }
      VarDeclNode *node = parser_alloc(p, sizeof(VarDeclNode));
      node->base.type = NODE_VAR_DECL;
      node->var_type = current_vtype;
      node->name = name_val;
      node->initializer = init;
      node->is_mutable = is_mut;
      node->is_array = is_array;
      node->array_size = array_size;
      set_loc((ASTNode*)node, line, col);
      
      *curr = (ASTNode*)node;
      curr = &node->base.next;
      
      if (p->current_token.type == TOKEN_COMMA) {
          eat(p, TOKEN_COMMA);
          
          next_extra_ptrs = 0;
          while (p->current_token.type == TOKEN_STAR) { if (p->has_error) break;
              next_extra_ptrs++;
              eat(p, TOKEN_STAR);
          }
          
          if (p->current_token.type != TOKEN_IDENTIFIER) parser_fail(p, "Expected identifier after comma");
          name_val = parser_strdup(p, p->current_token.text);
          p->current_token.text = NULL;
          eat(p, TOKEN_IDENTIFIER);
      } else {
          break;
      }
  }
  
  eat_semi(p);
  return head;
}