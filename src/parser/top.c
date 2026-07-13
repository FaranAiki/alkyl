#include "parser_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void apply_implicit_return(Parser *p, ASTNode **body_ptr) {
    if (!p->settings.allow_implicit_return || !body_ptr || !*body_ptr) return;
    ASTNode *last = *body_ptr;
    ASTNode *prev = NULL;
    while (last->next) {
        prev = last;
        last = last->next;
    }
    
    int is_expr = (last->type == NODE_LITERAL || last->type == NODE_VAR_REF || 
                   last->type == NODE_BINARY_OP || last->type == NODE_UNARY_OP || 
                   last->type == NODE_CALL || last->type == NODE_METHOD_CALL || 
                   last->type == NODE_ARRAY_ACCESS || last->type == NODE_MEMBER_ACCESS || 
                   last->type == NODE_CAST || last->type == NODE_INC_DEC);
    if (is_expr) {
        ReturnNode *ret = parser_alloc(p, sizeof(ReturnNode));
        ret->base.type = NODE_RETURN;
        ret->base.line = last->line;
        ret->base.col = last->col;
        ret->value = last;
        ret->base.next = NULL;
        
        if (prev) prev->next = (ASTNode*)ret;
        else *body_ptr = (ASTNode*)ret;
    }
}

ASTNode* parse_extern(Parser *p, int modifiers) {
  eat(p, TOKEN_EXTERN);
  // so that extern can be more
  if (p->current_token.type == TOKEN_CLASS || p->current_token.type == TOKEN_STRUCT || p->current_token.type == TOKEN_UNION|| p->current_token.type == TOKEN_IMPL || p->current_token.type == TOKEN_TRAIT ) {
      eat(p, p->current_token.type);
      if (p->current_token.type != TOKEN_IDENTIFIER) parser_fail(p, "Expected name for extern keyword");
      char *name = parser_strdup(p, p->current_token.text);
      eat(p, TOKEN_IDENTIFIER);
      eat(p, TOKEN_SEMICOLON);
      
      register_typename(p, name, 0); 
      
      ClassNode *cn = parser_alloc(p, sizeof(ClassNode));
      cn->base.type = NODE_CLASS;
      cn->name = name;
      cn->is_extern = 1; 
      apply_class_modifiers(cn, modifiers);
      return (ASTNode*)cn;
  }
  
  VarType ret_type = parse_type(p);
  if (ret_type.base == TYPE_UNKNOWN) { parser_fail(p, "Expected return type for extern function"); }
  if (p->current_token.type != TOKEN_IDENTIFIER) { parser_fail(p, "Expected extern function name"); }
  char *name = p->current_token.text; p->current_token.text = NULL; eat(p, TOKEN_IDENTIFIER);
  name = parser_strdup(p, name);

  eat(p, TOKEN_LPAREN);
  Parameter *params_head = NULL; Parameter **curr_param = &params_head;
  int is_varargs = 0;
  if (p->current_token.type != TOKEN_RPAREN) {
    while (1) {
      if (p->current_token.type == TOKEN_ELLIPSIS) { eat(p, TOKEN_ELLIPSIS); is_varargs = 1; break; }
      int pmods = parse_modifiers(p);
      VarType ptype = parse_type(p);
      if (ptype.base == TYPE_UNKNOWN) { parser_fail(p, "Expected parameter type"); }
      char *pname = NULL;
      if (p->current_token.type == TOKEN_IDENTIFIER) { pname = parser_strdup(p, p->current_token.text); p->current_token.text = NULL; eat(p, TOKEN_IDENTIFIER); }
      
      if (p->current_token.type == TOKEN_LBRACKET) {
          eat(p, TOKEN_LBRACKET);
          if (p->current_token.type != TOKEN_RBRACKET) {
              ASTNode *sz = parse_expression(p);
              (void)sz;
          }
          eat(p, TOKEN_RBRACKET);
          ptype.ptr_depth++;
      }
      
      Parameter *param = parser_alloc(p, sizeof(Parameter)); 
      apply_param_modifiers(param, pmods);
      param->type = ptype; param->name = pname; *curr_param = param; curr_param = &param->next;
      if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA); else break;
    }
  }
  eat(p, TOKEN_RPAREN); eat(p, TOKEN_SEMICOLON);
  FuncDefNode *node = parser_alloc(p, sizeof(FuncDefNode));
  node->base.type = NODE_FUNC_DEF; node->name = name; node->ret_type = ret_type;
  node->params = params_head; node->body = NULL; node->is_varargs = is_varargs;
  node->is_extern = true;
  node->cconv = p->pending_cconv ? p->pending_cconv : p->ctx->settings.default_cconv;
  p->pending_cconv = NULL;
  apply_func_modifiers(node, modifiers);
  return (ASTNode*)node;
}


ASTNode* parse_compound(Parser *p, int modifiers) {
  int line = p->current_token.line;
  int col = p->current_token.col;
  eat(p, TOKEN_COMPOUND);
  eat(p, TOKEN_LBRACKET);
  
  int max_params = 16;
  char **type_params = parser_alloc(p, sizeof(char*) * max_params);
  int num_params = 0;
  
  while (p->current_token.type != TOKEN_RBRACKET) {
      if (p->current_token.type == TOKEN_IDENTIFIER && p->current_token.text && strcmp(p->current_token.text, "type") == 0) {
          eat(p, TOKEN_IDENTIFIER);
      } else {
          parser_fail(p, "Expected 'type' keyword in compound");
      }
      
      if (p->current_token.type != TOKEN_IDENTIFIER) {
          parser_fail(p, "Expected type parameter name in compound");
      }
      char *type_param = parser_strdup(p, p->current_token.text);
      type_params[num_params++] = type_param;
      eat(p, TOKEN_IDENTIFIER);
      
      register_typename(p, type_param, 0);
      
      if (p->current_token.type == TOKEN_COMMA) {
          eat(p, TOKEN_COMMA);
      } else {
          break;
      }
  }
  
  eat(p, TOKEN_RBRACKET);
  
  ASTNode *body = NULL;
  if (p->current_token.type == TOKEN_LBRACE) {
      eat(p, TOKEN_LBRACE);
      body = parse_top_level(p); 
      eat(p, TOKEN_RBRACE);
  } else {
      body = parse_top_level(p); 
  }
  
  CompoundNode *cn = parser_alloc(p, sizeof(CompoundNode));
  cn->base.type = NODE_COMPOUND;
  cn->type_params = type_params;
  cn->num_type_params = num_params;
  cn->body = body;
  set_loc((ASTNode*)cn, line, col);
  return (ASTNode*)cn;
}

// MODULATE THIS WTF

ASTNode* parse_top_level_internal(Parser *p);

ASTNode* parse_top_level(Parser *p) {
    char *reason_str = NULL;
    if (p->current_token.type == TOKEN_REASON) {
        eat(p, TOKEN_REASON);
        if (p->current_token.type != TOKEN_STRING) parser_fail(p, "Expected string literal after reason");
        reason_str = parser_strdup(p, p->current_token.text);
        eat(p, TOKEN_STRING);
    }
    
    ASTNode *node = parse_top_level_internal(p);
    
    if (reason_str && node) {
        ASTNode *curr = node;
        while (curr) {
            curr->reason = reason_str;
            curr = curr->next;
        }
    }
    return node;
}

ASTNode* parse_top_level_internal(Parser *p) { 
  if (p->current_token.type == TOKEN_SEMICOLON) {
      eat(p, TOKEN_SEMICOLON);
      return NULL;
  }

  int modifiers = parse_modifiers(p);
  
  if (p->current_token.type == TOKEN_COMPOUND) {
      return parse_compound(p, modifiers);
  }

  if (p->current_token.type == TOKEN_NAMESPACE) {
      if (modifiers) parser_fail(p, "Modifiers not allowed on namespace");
      eat(p, TOKEN_NAMESPACE);
      if (p->current_token.type != TOKEN_IDENTIFIER) parser_fail(p, "Expected namespace name");
      char *ns_name = parser_strdup(p, p->current_token.text);
      eat(p, TOKEN_IDENTIFIER);

      eat(p, TOKEN_LBRACE);
      
      ASTNode *body_head = NULL;
      ASTNode **body_curr = &body_head;
      
      while(p->current_token.type != TOKEN_RBRACE && p->current_token.type != TOKEN_EOF) {
          ASTNode *n = parse_top_level(p);
          if (n) {
              *body_curr = n;
              while (*body_curr) body_curr = &(*body_curr)->next;
          }
      }
      eat(p, TOKEN_RBRACE);
      
      NamespaceNode *ns = parser_alloc(p, sizeof(NamespaceNode));
      ns->base.type = NODE_NAMESPACE;
      ns->name = ns_name;
      ns->body = body_head;
      return (ASTNode*)ns;
  }

  if (p->current_token.type == TOKEN_DEFINE) { if(modifiers) parser_fail(p, "Modifiers not allowed"); return parse_define(p); }
  if (p->current_token.type == TOKEN_TYPEDEF) { if(modifiers) parser_fail(p, "Modifiers not allowed"); return parse_typedef(p); }
  if (p->current_token.type == TOKEN_ENUM) { if(modifiers) parser_fail(p, "Modifiers not allowed"); return parse_enum(p); }

  if (p->current_token.type == TOKEN_PREMETA) {
      if(modifiers) parser_fail(p, "Modifiers not allowed");
      eat(p, TOKEN_PREMETA);
      eat(p, TOKEN_LBRACE);
      while(p->current_token.type != TOKEN_RBRACE && p->current_token.type != TOKEN_EOF) {
          char *reason_str = NULL;
          if (p->current_token.type == TOKEN_REASON) {
              eat(p, TOKEN_REASON);
              if (p->current_token.type != TOKEN_STRING) {
                  parser_fail(p, "Expected string literal after reason");
              }
              reason_str = parser_strdup(p, p->current_token.text);
              eat(p, TOKEN_STRING);
          }
          
          int line = p->current_token.line;
          int col = p->current_token.col;
          if (p->current_token.type == TOKEN_IDENTIFIER) {
              char *domain = parser_strdup(p, p->current_token.text);
              eat(p, TOKEN_IDENTIFIER);
              if (p->current_token.type == TOKEN_DOT) {
                  eat(p, TOKEN_DOT);
                  char *key = parser_strdup(p, p->current_token.text);
                  eat(p, TOKEN_IDENTIFIER);
                  eat(p, TOKEN_ASSIGN);
                  // parse value
                  char *val = NULL;
                  if (p->current_token.type == TOKEN_IDENTIFIER || p->current_token.type == TOKEN_NUMBER || p->current_token.type == TOKEN_STRING) {
                      val = parser_strdup(p, p->current_token.text);
                      eat(p, p->current_token.type);
                  }
                  
                  if (!reason_str) {
                      p->current_token.line = line;
                      p->current_token.col = col;
                      parser_fail(p, "no reason to set setting");
                  }
                  
                  if (strcmp(domain, "compiler") == 0) {
                      if (strcmp(key, "no_purge") == 0 && val) {
                          p->ctx->settings.no_purge = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
                      }
                  } else if (strcmp(domain, "lexer") == 0) {
                      if (strcmp(key, "scope_style") == 0 && val) {
                          if (strcmp(val, "SCOPE_INDENTATION") == 0) {
                              p->l->settings.scope_style = SCOPE_INDENTATION;
                          } else if (strcmp(val, "SCOPE_BRACKETS") == 0) {
                              p->l->settings.scope_style = SCOPE_BRACKETS;
                          } else {
                              parser_fail(p, "Unknown scope_style value");
                          }
                      } else if (strcmp(key, "require_semicolons") == 0 && val) {
                          p->l->settings.require_semicolons = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
                      }
                  }
              } else if (p->current_token.type == TOKEN_ASSIGN) {
                  eat(p, TOKEN_ASSIGN);
                  char *val = NULL;
                  if (p->current_token.type == TOKEN_IDENTIFIER || p->current_token.type == TOKEN_NUMBER || p->current_token.type == TOKEN_STRING) {
                      val = parser_strdup(p, p->current_token.text);
                      eat(p, p->current_token.type);
                  }
                  
                  if (!reason_str) {
                      p->current_token.line = line;
                      p->current_token.col = col;
                      parser_fail(p, "no reason to set setting");
                  }
                  
                  if (strcmp(domain, "cconv") == 0 && val) {
                      p->ctx->settings.default_cconv = val;
                  }
              }
          } else {
              eat(p, p->current_token.type); // skip unhandled
          }
          if (p->current_token.type == TOKEN_SEMICOLON) eat(p, TOKEN_SEMICOLON);
      }
      eat(p, TOKEN_RBRACE);
      return parse_top_level(p);
  }

  if (p->current_token.type == TOKEN_META || p->current_token.type == TOKEN_POSTMETA) {
      if(modifiers) parser_fail(p, "Modifiers not allowed");
      bool is_post = (p->current_token.type == TOKEN_POSTMETA);
      eat(p, p->current_token.type);
      
      if (p->current_token.type == TOKEN_LBRACKET) {
          eat(p, TOKEN_LBRACKET);
          while (p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF) {
              char *reason_str = NULL;
              if (p->current_token.type == TOKEN_REASON) {
                  eat(p, TOKEN_REASON);
                  if (p->current_token.type != TOKEN_STRING) {
                      parser_fail(p, "Expected string literal after reason");
                  }
                  reason_str = parser_strdup(p, p->current_token.text);
                  eat(p, TOKEN_STRING);
              }
              
              int line = p->current_token.line;
              int col = p->current_token.col;
              if (p->current_token.type == TOKEN_IDENTIFIER) {
                  char *key = parser_strdup(p, p->current_token.text);
                  eat(p, TOKEN_IDENTIFIER);
                  if (p->current_token.type == TOKEN_ASSIGN) {
                      eat(p, TOKEN_ASSIGN);
                      if (p->current_token.type == TOKEN_STRING || p->current_token.type == TOKEN_IDENTIFIER) {
                          if (!reason_str) {
                              p->current_token.line = line;
                              p->current_token.col = col;
                              parser_fail(p, "no reason to set setting");
                          }
                          if (strcmp(key, "cconv") == 0) {
                              p->pending_cconv = parser_strdup(p, p->current_token.text);
                          }
                          eat(p, p->current_token.type);
                      }
                  }
              } else {
                  eat(p, p->current_token.type);
              }
              if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA);
          }
          eat(p, TOKEN_RBRACKET);
          return parse_top_level(p); // parse the decorated element
      }
      
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
      return (ASTNode*)mn;
  }
 
  // TODO separate this
  if (p->current_token.type == TOKEN_CLASS || 
      p->current_token.type == TOKEN_STRUCT || 
      p->current_token.type == TOKEN_UNION || 
      (p->current_token.type == TOKEN_OPEN) || 
      (p->current_token.type == TOKEN_CLOSED)) {
    return parse_class_impl(p, modifiers);
  }

  if (p->current_token.type == TOKEN_LINK) { if(modifiers) parser_fail(p, "Modifiers not allowed"); return parse_link(p); }
  if (p->current_token.type == TOKEN_IMPORT) { if(modifiers) parser_fail(p, "Modifiers not allowed"); return parse_import(p); }
  if (p->current_token.type == TOKEN_EXTERN) return parse_extern(p, modifiers);

  if (p->current_token.type == TOKEN_KW_MUT || p->current_token.type == TOKEN_KW_IMUT) {
    ASTNode *var = parse_var_decl_internal(p);
    ASTNode *curr = var;
    while (curr) {
        apply_var_modifiers((VarDeclNode*)curr, modifiers);
        curr = curr->next;
    }
    return var;
  }

  int line = p->current_token.line;
  int col = p->current_token.col;

  int is_flux = 0;
  if (p->current_token.type == TOKEN_FLUX) {
      is_flux = 1;
      eat(p, TOKEN_FLUX);
  }

  VarType vtype = parse_type(p);
  if (vtype.base == TYPE_UNKNOWN) {
      if (modifiers) parser_fail(p, "Modifiers not allowed on statement");
      return parse_single_statement_or_block(p);
  }

  if (p->current_token.type == TOKEN_LPAREN) {
      char *name = NULL;
      vtype = parse_func_ptr_decl(p, vtype, &name);
      
      ASTNode *init = NULL;
      if (p->current_token.type == TOKEN_ASSIGN) {
          eat(p, TOKEN_ASSIGN);
          init = parse_expression(p);
      }
      eat(p, TOKEN_SEMICOLON);
      
      VarDeclNode *node = parser_alloc(p, sizeof(VarDeclNode));
      node->base.type = NODE_VAR_DECL;
      node->var_type = vtype;
      node->name = name;
      node->initializer = init;
      node->is_mutable = 1; 
      node->base.line = line; node->base.col = col;
      
      apply_var_modifiers(node, modifiers);
      return (ASTNode*)node;
  }

  if (p->current_token.type != TOKEN_IDENTIFIER) { parser_fail(p, "Expected identifier definition after type"); }
  char *name = parser_strdup(p, p->current_token.text); 
  p->current_token.text = NULL; eat(p, TOKEN_IDENTIFIER);
  
  if (p->settings.allow_postfix_types && p->current_token.type == TOKEN_COLON) {
      eat(p, TOKEN_COLON);
      VarType pt = parse_type(p);
      if (pt.base != TYPE_UNKNOWN) {
          vtype = pt;
      }
  }
  if (p->current_token.type == TOKEN_QUESTION) {
      Token next = parser_peek_token(p);
      if (next.type == TOKEN_LPAREN) {
          vtype.is_tainted = 1;
          eat(p, TOKEN_QUESTION);
      }
  }
  
  if (p->current_token.type == TOKEN_LPAREN) {
    eat(p, TOKEN_LPAREN);
    Parameter *params_head = NULL; Parameter **curr_param = &params_head;
    if (p->current_token.type != TOKEN_RPAREN) {
      while (1) {
        int pmods = parse_modifiers(p);
        VarType ptype = parse_type(p);
        if (ptype.base == TYPE_UNKNOWN) parser_fail(p, "Expected parameter type in function definition");
        char *pname = parser_strdup(p, p->current_token.text); 
        p->current_token.text = NULL; eat(p, TOKEN_IDENTIFIER);
        
        if (p->current_token.type == TOKEN_LBRACKET) {
            eat(p, TOKEN_LBRACKET);
            if (p->current_token.type != TOKEN_RBRACKET) {
                ASTNode *sz = parse_expression(p);
                (void)sz;
            }
            eat(p, TOKEN_RBRACKET);
            ptype.ptr_depth++;
        }
        
        Parameter *pm = parser_alloc(p, sizeof(Parameter)); 
        apply_param_modifiers(pm, pmods);
        pm->type = ptype; pm->name = pname; *curr_param = pm; curr_param = &pm->next;
        if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA); else break;
      }
    }
    eat(p, TOKEN_RPAREN); eat(p, TOKEN_LBRACE);
    ASTNode *body = parse_statements(p); eat(p, TOKEN_RBRACE);
    
    apply_implicit_return(p, &body);
    
    FuncDefNode *node = parser_alloc(p, sizeof(FuncDefNode));
    node->base.type = NODE_FUNC_DEF; node->name = name; node->ret_type = vtype; node->params = params_head; node->body = body;
    node->is_flux = is_flux; 
    node->base.line = line; node->base.col = col;
    node->cconv = p->pending_cconv ? p->pending_cconv : p->ctx->settings.default_cconv;
    p->pending_cconv = NULL; // Consume it
    
    apply_func_modifiers(node, modifiers);
    return (ASTNode*)node;
  } else {
    ASTNode *head = NULL;
    ASTNode **curr = &head;
    
    char *name_val = name;
    
    // Initial extra_ptrs for the first variable is 0
    int next_extra_ptrs = 0;
    
    while (1) {
        VarType current_vtype = vtype;
        current_vtype.ptr_depth += next_extra_ptrs;
        
        if (p->current_token.type == TOKEN_QUESTION) {
            current_vtype.is_tainted = 1;
            eat(p, TOKEN_QUESTION);
        }
        
        int is_array = 0;
        ASTNode *array_size = NULL;
        ASTNode **curr_sz = &array_size;
        
        while (p->current_token.type == TOKEN_LBRACKET) {
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

        ASTNode *init = NULL;
        if (p->current_token.type == TOKEN_ASSIGN) { eat(p, TOKEN_ASSIGN); init = parse_expression(p); } 
        else { if (current_vtype.base == TYPE_AUTO) { parser_fail(p, "'let' variable declaration must have an initializer"); } }
        
        VarDeclNode *node = parser_alloc(p, sizeof(VarDeclNode));
        node->base.type = NODE_VAR_DECL; node->var_type = current_vtype; node->name = name_val;
        node->initializer = init; node->is_mutable = 1; 
        node->is_array = is_array; node->array_size = array_size; 
        node->base.line = line; node->base.col = col;
        
        apply_var_modifiers(node, modifiers);
        *curr = (ASTNode*)node;
        curr = &node->base.next;
        
        if (p->current_token.type == TOKEN_COMMA) {
            eat(p, TOKEN_COMMA);
            
            // Allow pointer asterisks like *y
            next_extra_ptrs = 0;
              while (p->current_token.type == TOKEN_STAR) {
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
    eat(p, TOKEN_SEMICOLON);
    return head;
  }
}
