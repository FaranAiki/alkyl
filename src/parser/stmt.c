#include "parser_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void eat_semi(Parser *p) {
    if (p->current_token.type == TOKEN_SEMICOLON) {
        eat(p, TOKEN_SEMICOLON);
    } else if (p->current_token.type == TOKEN_ELSE || 
               p->current_token.type == TOKEN_ELIF || 
               p->current_token.type == TOKEN_RBRACE || 
               p->current_token.type == TOKEN_EOF) {
        // Implicit
    } else if (p->l->settings.require_semicolons == 0) {
        // Optional semicolon
    } else {
        eat(p, TOKEN_SEMICOLON);
    }
}

void set_loc(ASTNode *n, int line, int col) {
    if(n) { n->line = line; n->col = col; }
}

ASTNode* parse_return(Parser *p) {
  int line = p->current_token.line, col = p->current_token.col;
  eat(p, TOKEN_RETURN);
  ASTNode *val = NULL;
  if (p->current_token.type != TOKEN_SEMICOLON && 
      p->current_token.type != TOKEN_ELSE && 
      p->current_token.type != TOKEN_ELIF && 
      p->current_token.type != TOKEN_RBRACE && 
      p->current_token.type != TOKEN_EOF) {
    val = parse_expression(p);
  }
  eat_semi(p);
  ReturnNode *node = parser_alloc(p, sizeof(ReturnNode));
  node->base.type = NODE_RETURN;
  node->value = val;
  set_loc((ASTNode*)node, line, col);
  return (ASTNode*)node;
}

ASTNode* parse_emit(Parser *p) {
    int line = p->current_token.line, col = p->current_token.col;
    eat(p, TOKEN_EMIT);
    ASTNode *val = parse_expression(p);
    eat_semi(p);
    
    EmitNode *node = parser_alloc(p, sizeof(EmitNode));
    node->base.type = NODE_EMIT;
    node->value = val;
    set_loc((ASTNode*)node, line, col);
    return (ASTNode*)node;
}

ASTNode* parse_assignment_or_call(Parser *p) {
  Token start_token = p->current_token;
  if (start_token.text) start_token.text = parser_strdup(p, start_token.text); 

  int line = p->current_token.line;
  int col = p->current_token.col;

  char *name = p->current_token.text; // already arena alloc from lexer/strdup
  p->current_token.text = NULL; 
  eat(p, TOKEN_IDENTIFIER);
  
  ASTNode *node = parser_alloc(p, sizeof(VarRefNode));
  ((VarRefNode*)node)->base.type = NODE_VAR_REF;
  ((VarRefNode*)node)->name = name;
  set_loc(node, line, col);



  node = parse_postfix(p, node);

  int is_assign = 0;
  switch (p->current_token.type) {
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
    int op = p->current_token.type;
    eat(p, op); 
    ASTNode *expr = parse_expression(p);
    eat_semi(p); 

    AssignNode *an = parser_alloc(p, sizeof(AssignNode));
    an->base.type = NODE_ASSIGN;
    an->value = expr;
    an->op = op;
    
    if (node->type == NODE_VAR_REF) {
        an->name = ((VarRefNode*)node)->name;
        ((VarRefNode*)node)->name = NULL; 
        // No free(node)
    } else {
        an->target = node; 
    }
    set_loc((ASTNode*)an, line, col);
    // No free(start_token.text)
    return (ASTNode*)an;
  }
  
  if (node->type == NODE_VAR_REF || node->type == NODE_TEMPLATE_INSTANTIATION) {
      TokenType t = p->current_token.type;
      int is_arg_start = (t == TOKEN_NUMBER || t == TOKEN_FLOAT || t == TOKEN_STRING || 
            t == TOKEN_CHAR_LIT || t == TOKEN_TRUE || t == TOKEN_FALSE || 
            t == TOKEN_IDENTIFIER || t == TOKEN_LPAREN || t == TOKEN_LBRACKET || 
            t == TOKEN_NOT || t == TOKEN_BIT_NOT || t == TOKEN_MINUS || t == TOKEN_PLUS || t == TOKEN_STAR || t == TOKEN_AND || t == TOKEN_TYPEOF);

      if (is_arg_start) {
          char *fname = NULL;
          if (node->type == NODE_VAR_REF) {
              fname = ((VarRefNode*)node)->name;
          } else {
              TemplateInstNode *ti = (TemplateInstNode*)node;
              if (ti->target->type == NODE_VAR_REF) {
                  fname = ((VarRefNode*)ti->target)->name;
              }
          }
          
          ASTNode *args_head = NULL;
          ASTNode **curr_arg = &args_head;
          
          *curr_arg = parse_expression(p);
          curr_arg = &(*curr_arg)->next;

          while (p->current_token.type == TOKEN_COMMA) {
              eat(p, TOKEN_COMMA);
              *curr_arg = parse_expression(p);
              curr_arg = &(*curr_arg)->next;
          }
          eat_semi(p); 

          CallNode *cn = parser_alloc(p, sizeof(CallNode));
          cn->base.type = NODE_CALL;
          cn->name = fname;
          cn->target = node; // Store the original node (e.g. VarRef or TemplateInst)
          cn->args = args_head;
          
          // If it was a template instantiation, wrap it or handle it!
          // Wait! Semantic analyzer looks at CallNode's `name` only!
          // But if it's a template instantiation, `name` is the original template name...
          // BUT wait! In expr.c (postfix), if it's `map[int]`, `parse_postfix` wraps the VarRef in TemplateInstNode.
          // In an expression statement, we should probably set `cn->name` to `fname`, BUT `map[int]` is lost?
          // No! If `cn->name` is just `map`, it won't know `[int]`!
          
          // Oh, wait! The Semantic Analyzer currently treats `map[int](...)` as a CallNode with name = `map`. Wait, no, `expr.c` line 80 parse_postfix gives `TemplateInstNode`.
          // Let's check how `expr.c` parses `map[int]()`!
          set_loc((ASTNode*)cn, line, col);
          // No free
          return (ASTNode*)cn;
      }
  }
  if (p->current_token.type == TOKEN_SEMICOLON || 
      p->current_token.type == TOKEN_ELSE || 
      p->current_token.type == TOKEN_ELIF || 
      p->current_token.type == TOKEN_RBRACE || 
      p->current_token.type == TOKEN_EOF) {
      
      eat_semi(p);
      // No free
      return node; 
  }
  
  char msg[256];
  snprintf(msg, sizeof(msg), "Invalid statement starting with identifier '%s'.", 
           ((VarRefNode*)node)->name);
  
  const char *keyword_suggestion = find_closest_keyword(((VarRefNode*)node)->name);
  
  // Custom fail reporting to avoid exit
  report_error(p->l, start_token, msg);
  if (keyword_suggestion) {
      char hint[128];
      snprintf(hint, sizeof(hint), "Did you mean %s?", keyword_suggestion);
      report_hint(p->l, start_token, hint);
  }

  // No frees needed
  
  if (p->ctx) p->ctx->error_count++;
  
  if (p->recover_buf) longjmp(*p->recover_buf, 1);
  else exit(1);

  return NULL;
}

ASTNode* parse_single_statement_or_block_internal(Parser *p);

ASTNode* parse_single_statement_or_block(Parser *p) {
    char *reason_str = NULL;
    if (p->current_token.type == TOKEN_REASON) {
        eat(p, TOKEN_REASON);
        if (p->current_token.type != TOKEN_STRING) parser_fail(p, "Expected string literal after reason");
        reason_str = parser_strdup(p, p->current_token.text);
        eat(p, TOKEN_STRING);
    }
    
    ASTNode *node = parse_single_statement_or_block_internal(p);
    
    if (reason_str && node) {
        ASTNode *curr = node;
        while (curr) {
            curr->reason = reason_str;
            curr = curr->next;
        }
    }
    return node;
}

ASTNode* parse_single_statement_or_block_internal(Parser *p) {
  if (p->current_token.type == TOKEN_LBRACE) {
    eat(p, TOKEN_LBRACE);
    ASTNode *block = parse_statements(p);
    eat(p, TOKEN_RBRACE);
    return block;
  }
  
  int line = p->current_token.line, col = p->current_token.col;

  // Handle explicit non-modifier WASH and CLEAN statements
  if (p->current_token.type == TOKEN_WASH || p->current_token.type == TOKEN_CLEAN) {
      WashType wash_type = (p->current_token.type == TOKEN_CLEAN) ? WASH_TYPE_CLEAN : WASH_TYPE_WASH;
      eat(p, p->current_token.type);
      ASTNode *target = parse_expression(p);
      return parse_wash_or_clean_tail(p, target, wash_type); 
  }

  // Handle explicit UNTAINT statement
  if (p->current_token.type == TOKEN_UNTAINT) {
      eat(p, TOKEN_UNTAINT);
      ASTNode *target = parse_expression(p);
      return parse_wash_or_clean_tail(p, target, WASH_TYPE_UNTAINT);
  }

  int modifiers = parse_modifiers(p);

  if (p->current_token.type == TOKEN_LOOP) { if(modifiers) parser_fail(p, "Invalid modifier here"); return parse_loop(p); }
  if (p->current_token.type == TOKEN_WHILE) { if(modifiers) parser_fail(p, "Invalid modifier here"); return parse_while(p); }
  if (p->current_token.type == TOKEN_IF) { if(modifiers) parser_fail(p, "Invalid modifier here"); return parse_if(p); }
  if (p->current_token.type == TOKEN_SWITCH) { if(modifiers) parser_fail(p, "Invalid modifier here"); return parse_switch(p); }
  if (p->current_token.type == TOKEN_RETURN) { if(modifiers) parser_fail(p, "Invalid modifier here"); return parse_return(p); }
  if (p->current_token.type == TOKEN_BREAK) { if(modifiers) parser_fail(p, "Invalid modifier here"); return parse_break(p); }
  if (p->current_token.type == TOKEN_CONTINUE) { if(modifiers) parser_fail(p, "Invalid modifier here"); return parse_continue(p); }
  
  if (p->current_token.type == TOKEN_EMIT) { if(modifiers) parser_fail(p, "Invalid modifier here"); return parse_emit(p); }
  if (p->current_token.type == TOKEN_PURGE) { 
      if(modifiers) parser_fail(p, "Invalid modifier here"); 
      eat(p, TOKEN_PURGE);
      PurgeNode *n = parser_alloc(p, sizeof(PurgeNode));
      n->base.type = NODE_PURGE;
      n->base.line = line;
      n->base.col = col;
      n->msg = parse_expression(p);
      eat_semi(p);
      return (ASTNode*)n;
  }
  if (p->current_token.type == TOKEN_FOR) { if(modifiers) parser_fail(p, "Invalid modifier here"); return parse_for_in(p); }
  if (p->current_token.type == TOKEN_DEFINE) { 
      if(modifiers) parser_fail(p, "Modifiers not allowed"); 
      extern ASTNode* parse_define(Parser *p);
      return parse_define(p); 
  }
  if (p->current_token.type == TOKEN_EXTERN) {
      extern ASTNode* parse_extern(Parser *p, int modifiers);
      return parse_extern(p, modifiers);
  }

  if (p->current_token.type == TOKEN_DEFER) {
      if(modifiers) parser_fail(p, "Modifiers not allowed");
      eat(p, TOKEN_DEFER);
      
      ASTNode *body;
      if (p->current_token.type == TOKEN_LBRACE) {
          eat(p, TOKEN_LBRACE);
          body = parse_statements(p);
          eat(p, TOKEN_RBRACE);
      } else {
          body = parse_single_statement_or_block(p);
      }
      
      DeferNode *dn = parser_alloc(p, sizeof(DeferNode));
      dn->base.type = NODE_DEFER;
      dn->body = body;
      set_loc((ASTNode*)dn, line, col);
      return (ASTNode*)dn;
  }

  if (p->current_token.type == TOKEN_META || p->current_token.type == TOKEN_POSTMETA) {
      if(modifiers) parser_fail(p, "Modifiers not allowed");
      bool is_post = (p->current_token.type == TOKEN_POSTMETA);
      eat(p, p->current_token.type);
      
      ASTNode *body_head = NULL;
      if (p->current_token.type == TOKEN_IF) {
          body_head = parse_if(p);
      } else if (p->current_token.type == TOKEN_WHILE) {
          body_head = parse_while(p);
      } else {
          eat(p, TOKEN_LBRACE);
          body_head = parse_statements(p);
          eat(p, TOKEN_RBRACE);
      }
      
      MetaNode *mn = parser_alloc(p, sizeof(MetaNode));
      mn->base.type = is_post ? NODE_POSTMETA : NODE_META;
      mn->is_post = is_post;
      mn->body = body_head;
      set_loc((ASTNode*)mn, line, col);
      return (ASTNode*)mn;
  }

  VarType peek_t = parse_type(p); 
  if (peek_t.base != TYPE_UNKNOWN) {
      if (peek_t.base == TYPE_CLASS && p->current_token.type == TOKEN_LPAREN) {
          if(modifiers) parser_fail(p, "Invalid modifier here");
          VarRefNode *vn = parser_alloc(p, sizeof(VarRefNode));
          vn->base.type = NODE_VAR_REF;
          vn->name = peek_t.class_name;
          ASTNode* call = parse_call(p, (ASTNode*)vn);
          eat(p, TOKEN_SEMICOLON);
          set_loc(call, line, col);
          return call;
      }
      
      if (p->current_token.type == TOKEN_LPAREN) {
          char *name = NULL;
          VarType fp_type = parse_func_ptr_decl(p, peek_t, &name);
          
          ASTNode *init = NULL;
          if (p->current_token.type == TOKEN_ASSIGN) {
              eat(p, TOKEN_ASSIGN);
              init = parse_expression(p);
          }
          eat_semi(p);
          
          VarDeclNode *node = parser_alloc(p, sizeof(VarDeclNode));
          node->base.type = NODE_VAR_DECL;
          node->var_type = fp_type;
          node->name = name;
          node->initializer = init;
          node->is_mutable = 1; 
          set_loc((ASTNode*)node, line, col);
          
          apply_var_modifiers(node, modifiers);
          return (ASTNode*)node;
      }

      int is_mut = 1;
      if (p->current_token.type == TOKEN_KW_MUT) { is_mut = 1; eat(p, TOKEN_KW_MUT); }
      else if (p->current_token.type == TOKEN_KW_IMUT) { is_mut = 0; eat(p, TOKEN_KW_IMUT); }
      
      if (p->current_token.type != TOKEN_IDENTIFIER) parser_fail(p, "Expected variable name in declaration");
      char *name = p->current_token.text;
      p->current_token.text = NULL;
      eat(p, TOKEN_IDENTIFIER);
      
      if (p->settings.allow_postfix_types && p->current_token.type == TOKEN_COLON) {
          eat(p, TOKEN_COLON);
          VarType pt = parse_type(p);
          if (pt.base != TYPE_UNKNOWN) {
              peek_t = pt;
          }
      }
      
      int is_array = 0;
      ASTNode *array_size = NULL;
      ASTNode **curr_sz = &array_size;
      
      while (p->current_token.type == TOKEN_LBRACKET) {
        is_array = 1;
        peek_t.ptr_depth++; 
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

      ASTNode *init = NULL;
      if (p->current_token.type == TOKEN_ASSIGN) {
        eat(p, TOKEN_ASSIGN);
        init = parse_expression(p);
      }
      eat_semi(p);
      VarDeclNode *node = parser_alloc(p, sizeof(VarDeclNode));
      node->base.type = NODE_VAR_DECL;
      node->var_type = peek_t;
      node->name = name;
      node->initializer = init;
      node->is_mutable = is_mut;
      node->is_array = is_array;
      node->array_size = array_size;
      set_loc((ASTNode*)node, line, col);
      
      apply_var_modifiers(node, modifiers);
      return (ASTNode*)node;
  }
  
  if (p->current_token.type == TOKEN_KW_MUT || p->current_token.type == TOKEN_KW_IMUT) {
      ASTNode *var = parse_var_decl_internal(p);
      ASTNode *curr = var;
      while (curr) {
          apply_var_modifiers((VarDeclNode*)curr, modifiers);
          curr = curr->next;
      }
      return var;
  }

  if (modifiers) parser_fail(p, "Invalid modifier on statement");

  if (p->current_token.type == TOKEN_IDENTIFIER) return parse_assignment_or_call(p);
  
  ASTNode *expr = parse_expression(p);
  eat_semi(p); 
  return expr;
}

ASTNode* parse_statements(Parser *p) {
  ASTNode *head = NULL;
  ASTNode **current = &head;
  while (p->current_token.type != TOKEN_EOF && p->current_token.type != TOKEN_RBRACE) {
    ASTNode *stmt = parse_single_statement_or_block(p);
    if (stmt) {
      *current = stmt;
      while (*current) {
          current = &(*current)->next;
      }
    }
  }
  return head;
}
