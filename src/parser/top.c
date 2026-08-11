#include "parser_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

Token parser_peek_token_n(Parser *p, int offset);

// TODO modularize this
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
                   last->type == NODE_INDEX_ACCESS || last->type == NODE_MEMBER_ACCESS ||
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

static ASTNode* parse_single_extern(Parser *p, int modifiers) {
  modifiers |= parse_modifiers(p);
  
  if (p->current_token.type == TOKEN_CLASS || p->current_token.type == TOKEN_STRUCT || p->current_token.type == TOKEN_UNION) {
      eat(p, p->current_token.type);
      if (p->has_error) return NULL;
      if (p->current_token.type != TOKEN_IDENTIFIER) { parser_fail(p, "Expected name for extern keyword"); return NULL; }
      char *name = parser_strdup(p, p->current_token.text);
      eat(p, TOKEN_IDENTIFIER);
      if (p->has_error) return NULL;
      eat_semi(p);

      register_typename(p, name, 0);

      ClassNode *cn = parser_alloc(p, sizeof(ClassNode));
      cn->base.type = NODE_CLASS;
      cn->name = name;
      cn->is_extern = 1;
      apply_class_modifiers(cn, modifiers);
      return (ASTNode*)cn;
  }

  VarType ret_type = parse_type(p);
  if (ret_type.base == TYPE_UNKNOWN) { parser_fail(p, "Expected return type for extern function"); return NULL; }
  if (p->current_token.type != TOKEN_IDENTIFIER) { parser_fail(p, "Expected extern function name"); return NULL; }
  char *name = p->current_token.text; p->current_token.text = NULL; eat(p, TOKEN_IDENTIFIER);
  if (p->has_error) return NULL;
  name = parser_strdup(p, name);

  eat(p, TOKEN_LPAREN);
  Parameter *params_head = NULL; Parameter **curr_param = &params_head;
  int is_varargs = 0;
  if (p->current_token.type != TOKEN_RPAREN) {
    while (1) { if (p->has_error) break;
      if (p->current_token.type == TOKEN_ELLIPSIS) { eat(p, TOKEN_ELLIPSIS); is_varargs = 1; break; }
      int pmods = parse_modifiers(p);
      VarType ptype = parse_type(p);
      if (ptype.base == TYPE_UNKNOWN) { parser_fail(p, "Expected parameter type"); return NULL; }
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

      Parameter *param = parser_alloc_raw(p, sizeof(Parameter));
      apply_param_modifiers(param, pmods);
      param->type = ptype; param->name = pname;

      if (p->current_token.type == TOKEN_ASSIGN) {
          eat(p, TOKEN_ASSIGN);
          param->default_value = parse_expression(p);
      } else {
          param->default_value = NULL;
      }

      *curr_param = param; curr_param = &param->next;
      if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA); else break;
    }
  }
  eat(p, TOKEN_RPAREN);
  if (p->has_error) return NULL;

  char *extern_name = NULL;
  if (p->current_token.type == TOKEN_AS) {
      eat(p, TOKEN_AS);
      if (p->has_error) return NULL;
      if (p->current_token.type != TOKEN_IDENTIFIER) { parser_fail(p, "Expected identifier after 'as'"); return NULL; }
      extern_name = name;
      name = parser_strdup(p, p->current_token.text);
      eat(p, TOKEN_IDENTIFIER);
      if (p->has_error) return NULL;
  }

  eat_semi(p);
  FuncDefNode *node = parser_alloc(p, sizeof(FuncDefNode));
  node->base.type = NODE_FUNC_DEF; node->name = name; node->ret_type = ret_type;
  node->params = params_head; node->body = NULL; node->is_varargs = is_varargs;
  node->has_body = 0;
  node->is_extern = true;
  node->extern_name = extern_name;
  node->cconv = p->pending_cconv ? p->pending_cconv : p->ctx->settings.default_cconv;
  p->pending_cconv = NULL;
  apply_func_modifiers(node, modifiers);
  return (ASTNode*)node;
}

ASTNode* parse_extern(Parser *p, int modifiers) {
  eat(p, TOKEN_EXTERN);

  modifiers |= parse_modifiers(p);

  if (p->current_token.type == TOKEN_LBRACE) {
      eat(p, TOKEN_LBRACE);
      ASTNode *head = NULL;
      ASTNode **curr = &head;

      while (p->current_token.type != TOKEN_RBRACE && p->current_token.type != TOKEN_EOF) { if (p->has_error) break;
          ASTNode *decl = parse_single_extern(p, modifiers);
          if (decl) {
              if (!head) {
                  head = decl;
                  curr = &decl->next;
              } else {
                  *curr = decl;
                  curr = &decl->next;
              }
              while (*curr) curr = &(*curr)->next;
          }
      }
      eat(p, TOKEN_RBRACE);
      return head;
  }

  return parse_single_extern(p, modifiers);
}


ASTNode* parse_compound(Parser *p, int modifiers) {
  int line = p->current_token.line;
  int col = p->current_token.col;
  (void)modifiers;
  eat(p, TOKEN_COMPOUND);

  TokenType end_token = TOKEN_RBRACKET;
  if (p->current_token.type == TOKEN_LT) {
      end_token = TOKEN_GT;
      eat(p, TOKEN_LT);
  } else {
      eat(p, TOKEN_LBRACKET);
  }

  int max_params = 16;
  char **type_params = parser_alloc_raw(p, sizeof(char*) * max_params);
  VarType **allowed_types = parser_alloc_raw(p, sizeof(VarType*) * max_params);
  int *num_allowed = parser_alloc_raw(p, sizeof(int) * max_params);
  int num_params = 0;

  while (p->current_token.type != end_token) { if (p->has_error) break;
      VarType *curr_allowed = NULL;
      int curr_num = 0;
      if (p->current_token.type == TOKEN_IDENTIFIER && p->current_token.text && streq_lit(p->current_token.text, "type")) {
          eat(p, TOKEN_IDENTIFIER);
          if (p->current_token.type == TOKEN_LBRACKET || p->current_token.type == TOKEN_LT) {
              TokenType inner_end_token = (p->current_token.type == TOKEN_LBRACKET) ? TOKEN_RBRACKET : TOKEN_GT;
              eat(p, p->current_token.type);
              curr_allowed = parser_alloc_raw(p, sizeof(VarType) * 16);
              while (p->current_token.type != inner_end_token && p->current_token.type != TOKEN_EOF) { if (p->has_error) break;
                  curr_allowed[curr_num++] = parse_type(p);
                  if (p->current_token.type == TOKEN_COMMA) {
                      eat(p, TOKEN_COMMA);
                  } else {
                      break;
                  }
              }
              eat(p, inner_end_token);
          }
      } else {
          parser_fail(p, "Expected 'type' keyword in compound");
      }

      if (p->current_token.type != TOKEN_IDENTIFIER) {
          parser_fail(p, "Expected type parameter name in compound");
      }
      char *type_param = parser_strdup(p, p->current_token.text);
      type_params[num_params] = type_param;
      allowed_types[num_params] = curr_allowed;
      num_allowed[num_params] = curr_num;
      num_params++;
      eat(p, TOKEN_IDENTIFIER);

      register_typename(p, type_param, 0);

      if (p->current_token.type == TOKEN_COMMA) {
          eat(p, TOKEN_COMMA);
      } else {
          break;
      }
  }

  eat(p, end_token);

  ASTNode *body = NULL;
  if (p->current_token.type == TOKEN_LBRACE) {
      eat(p, TOKEN_LBRACE);
      ASTNode **curr_body = &body;
      while (p->current_token.type != TOKEN_RBRACE && p->current_token.type != TOKEN_EOF) { if (p->has_error) break;
          ASTNode *stmt = parse_top_level(p);
          if (stmt) {
              *curr_body = stmt;
              while (*curr_body) {
                  curr_body = &(*curr_body)->next;
              }
          }
      }
      eat(p, TOKEN_RBRACE);
  } else {
      body = parse_top_level(p);
  }

  CompoundNode *cn = parser_alloc(p, sizeof(CompoundNode));
  cn->base.type = NODE_COMPOUND;
  cn->type_params = type_params;
  cn->allowed_types = allowed_types;
  cn->num_allowed = num_allowed;
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
        if (p->current_token.type != TOKEN_STRING && p->current_token.type != TOKEN_C_STRING) {
            parser_fail(p, "Expected string literal after reason");
            return NULL;
        }
        reason_str = parser_strdup(p, p->current_token.text);
        eat(p, p->current_token.type);
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

ASTNode* parse_func_def_after_type(Parser *p, int modifiers, VarType vtype, int line, int col, int is_flux);

// Parses `errnum [ErrA, ErrB, ...]` which MUST be immediately followed by a
// function definition (no semicolon). The error set is attached to that
// function, marking its return type as tainted and recording the error names.
ASTNode* parse_errnum(Parser *p) {
    int line = p->current_token.line;
    int col = p->current_token.col;
    eat(p, TOKEN_ERRNUM);
    eat(p, TOKEN_LBRACKET);

    EnumEntry *head = NULL;
    EnumEntry **curr = &head;

    while (p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF) { if (p->has_error) break;
        if (p->current_token.type != TOKEN_IDENTIFIER) {
            parser_fail(p, "Expected identifier in errnum list");
            break;
        }

        EnumEntry *entry = parser_alloc_raw(p, sizeof(EnumEntry));
        entry->name = parser_strdup(p, p->current_token.text);
        entry->value = -1; // Semantic analyzer handles numbering
        entry->next = NULL;
        *curr = entry;
        curr = &entry->next;

        eat(p, TOKEN_IDENTIFIER);
        if (p->current_token.type == TOKEN_COMMA) {
            eat(p, TOKEN_COMMA);
        } else {
            break;
        }
    }
    eat(p, TOKEN_RBRACKET);

    // The errnum must be followed directly by a function definition.
    if (p->current_token.type == TOKEN_SEMICOLON) {
        parser_fail(p, "errnum must be attached to a function declaration (no semicolon)");
        return NULL;
    }

    int modifiers = parse_modifiers(p);
    VarType vtype = parse_type(p);
    if (vtype.base == TYPE_UNKNOWN) {
        parser_fail(p, "errnum must be followed by a function definition");
        return NULL;
    }
    if (p->current_token.type == TOKEN_FLUX) {
        eat(p, TOKEN_FLUX);
    }

    ASTNode *fn = parse_func_def_after_type(p, modifiers, vtype, line, col, 0);
    if (!fn || fn->type != NODE_FUNC_DEF) {
        parser_fail(p, "errnum must be followed by a function definition");
        return fn;
    }

    // Attach the error set to the function definition.
    FuncDefNode *fd = (FuncDefNode*)fn;
    int count = 0;
    for (EnumEntry *e = head; e; e = e->next) count++;
    fd->err_names = parser_alloc_raw(p, sizeof(char*) * (count > 0 ? count : 1));
    fd->num_err = count;
    fd->has_errnum = 1;
    int i = 0;
    for (EnumEntry *e = head; e; e = e->next) {
        fd->err_names[i++] = e->name;
    }
    // The function's return type is tainted because it can fail with one of these errors.
    fd->ret_type.is_tainted = 1;

    // Free the temporary EnumEntry list (names are now owned by the FuncDefNode).
    return fn;
}

ASTNode* parse_top_level_internal(Parser *p) {
  if (p->current_token.type == TOKEN_SEMICOLON) {
      eat_semi(p);
      return NULL;
  }

  if (p->current_token.type == TOKEN_AT) {
      Token after_at = parser_peek_token(p);
      if (after_at.type == TOKEN_IDENTIFIER && streq_lit(after_at.text, "c")) {
          Token after_c = parser_peek_token_n(p, 1);
          if (after_c.type == TOKEN_IMPORT) {
              eat(p, TOKEN_AT);
              eat(p, TOKEN_IDENTIFIER);
              eat(p, TOKEN_IMPORT);
              if (p->current_token.type == TOKEN_LPAREN) {
                  eat(p, TOKEN_LPAREN);
                  ASTNode *path_expr = parse_expression(p);
                  eat(p, TOKEN_RPAREN);
                  ImportExprNode *ie = parser_alloc(p, sizeof(ImportExprNode));
                  ie->base.type = NODE_IMPORT_EXPR;
                  ie->path = NULL;
                  if (path_expr && path_expr->type == NODE_LITERAL && ((LiteralNode*)path_expr)->var_type.base == TYPE_CHAR && ((LiteralNode*)path_expr)->var_type.ptr_depth == 1) {
                      ie->path = parser_strdup(p, ((LiteralNode*)path_expr)->val.str_val);
                  }
                  if (p->current_token.type == TOKEN_SEMICOLON) eat_semi(p);
                  return (ASTNode*)ie;
              }
              char* fname = NULL;
              if (p->current_token.type == TOKEN_STRING || p->current_token.type == TOKEN_C_STRING) {
                  fname = parser_strdup(p, p->current_token.text);
                  eat(p, p->current_token.type);
              } else if (p->current_token.type == TOKEN_IDENTIFIER) {
                  fname = parser_strdup(p, p->current_token.text);
                  eat(p, TOKEN_IDENTIFIER);
              } else {
                  parser_fail(p, "Expected file path after '@c import'");
                  return NULL;
              }
              if (p->current_token.type == TOKEN_SEMICOLON) eat_semi(p);
              ImportNode *imp = parser_alloc(p, sizeof(ImportNode));
              imp->base.type = NODE_IMPORT;
              imp->base.line = p->current_token.line;
              imp->base.col = p->current_token.col;
              imp->path = fname;
              imp->resolved_body = NULL;
              imp->header = HEADER_C;
              return (ASTNode*)imp;
          }
      }
  }

  int modifiers = parse_modifiers(p);

  if (p->current_token.type == TOKEN_LBRACE && modifiers != 0) {
      eat(p, TOKEN_LBRACE);
      ASTNode *head = NULL;
      ASTNode **curr = &head;
      while (p->current_token.type != TOKEN_RBRACE && p->current_token.type != TOKEN_EOF) {
          if (p->has_error) break;
          ASTNode *n = parse_top_level(p);
          if (n) {
              ASTNode *n_curr = n;
              while (n_curr) {
                  apply_modifiers_to_node(n_curr, modifiers);
                  n_curr = n_curr->next;
              }
              *curr = n;
              while (*curr) curr = &(*curr)->next;
          }
      }
      eat(p, TOKEN_RBRACE);
      return head;
  }

  if (p->current_token.type == TOKEN_COMPOUND) {
      return parse_compound(p, modifiers);
  }

  if (p->current_token.type == TOKEN_NAMESPACE) {
      eat(p, TOKEN_NAMESPACE);
      
      char *ns_name = NULL;
      if (p->current_token.type == TOKEN_IDENTIFIER) {
          ns_name = parser_strdup(p, p->current_token.text);
          eat(p, TOKEN_IDENTIFIER);
      } else if (p->current_token.type == TOKEN_LBRACE) {
          char buf[64];
          static int anon_ns_counter = 0;
          snprintf(buf, sizeof(buf), "__anon_ns_%d", ++anon_ns_counter);
          ns_name = parser_strdup(p, buf);
          modifiers |= MODIFIER_PRIVATE;
      } else {
          parser_fail(p, "Expected namespace name or '{'");
          return NULL;
      }

      char *old_ns = parser_strdup(p, diag_get_namespace(p->ctx));
      diag_set_namespace(p->ctx, ns_name);

      eat(p, TOKEN_LBRACE);

      ASTNode *body_head = NULL;
      ASTNode **body_curr = &body_head;

      while(p->current_token.type != TOKEN_RBRACE && p->current_token.type != TOKEN_EOF) { if (p->has_error) break;
          ASTNode *n = parse_top_level(p);
          if (n) {
              *body_curr = n;
              while (*body_curr) body_curr = &(*body_curr)->next;
          }
      }
      eat(p, TOKEN_RBRACE);

      diag_set_namespace(p->ctx, old_ns);

      NamespaceNode *ns = parser_alloc(p, sizeof(NamespaceNode));
      ns->base.type = NODE_NAMESPACE;
      ns->name = ns_name;
      ns->body = body_head;
      
      ns->is_open = 1; // default
      if (modifiers & MODIFIER_CLOSED) {
          ns->is_open = 0;
          ns->is_closed = 1;
      }
      if (modifiers & MODIFIER_PRIVATE) {
          ns->is_private = 1;
      }
      return (ASTNode*)ns;
  }

  if (p->current_token.type == TOKEN_EXPORT && parser_peek_token(p).type == TOKEN_NAMESPACE) {
      eat(p, TOKEN_EXPORT);
      eat(p, TOKEN_NAMESPACE);
      
      char *ns_name = NULL;
      if (p->current_token.type == TOKEN_IDENTIFIER) {
          ns_name = parser_strdup(p, p->current_token.text);
          eat(p, TOKEN_IDENTIFIER);
      } else if (p->current_token.type == TOKEN_LBRACE) {
          char buf[64];
          static int anon_ns_counter = 0;
          snprintf(buf, sizeof(buf), "__anon_ns_%d", ++anon_ns_counter);
          ns_name = parser_strdup(p, buf);
      } else {
          parser_fail(p, "Expected namespace name or '{'");
          return NULL;
      }

      char *old_ns = parser_strdup(p, diag_get_namespace(p->ctx));
      diag_set_namespace(p->ctx, ns_name);

      eat(p, TOKEN_LBRACE);

      ASTNode *body_head = NULL;
      ASTNode **body_curr = &body_head;

      while(p->current_token.type != TOKEN_RBRACE && p->current_token.type != TOKEN_EOF) { if (p->has_error) break;
          ASTNode *n = parse_top_level(p);
          if (n) {
              *body_curr = n;
              while (*body_curr) body_curr = &(*body_curr)->next;
          }
      }
      eat(p, TOKEN_RBRACE);

      diag_set_namespace(p->ctx, old_ns);

      NamespaceNode *ns = parser_alloc(p, sizeof(NamespaceNode));
      ns->base.type = NODE_NAMESPACE;
      ns->name = ns_name;
      ns->body = body_head;
      
      ns->is_open = 1;
      ns->is_closed = 0;
      ns->is_private = 0;
      ns->is_exported = 1;
      return (ASTNode*)ns;
  }

  if (p->current_token.type == TOKEN_DEFINE) { if(modifiers) parser_fail(p, "Modifiers not allowed"); return parse_define(p); }
  if (p->current_token.type == TOKEN_TYPEDEF) { if(modifiers) parser_fail(p, "Modifiers not allowed"); return parse_typedef(p); }
  if (p->current_token.type == TOKEN_ENUM) { if(modifiers) parser_fail(p, "Modifiers not allowed"); return parse_enum(p); }
  if (p->current_token.type == TOKEN_ERRNUM) {
      if (modifiers) parser_fail(p, "Modifiers not allowed on errnum");
      return parse_errnum(p);
  }

  if (p->current_token.type == TOKEN_PREMETA) {
      if(modifiers) parser_fail(p, "Modifiers not allowed");
      eat(p, TOKEN_PREMETA);
      eat(p, TOKEN_LBRACE);
       while(p->current_token.type != TOKEN_RBRACE && p->current_token.type != TOKEN_EOF) { if (p->has_error) break;
           char *reason_str = NULL;
          if (p->current_token.type == TOKEN_REASON) {
              eat(p, TOKEN_REASON);
              if (p->current_token.type != TOKEN_STRING && p->current_token.type != TOKEN_C_STRING) {
                  parser_fail(p, "Expected string literal after reason");
              }
              reason_str = parser_strdup(p, p->current_token.text);
              eat(p, p->current_token.type);
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
                   if (p->current_token.type == TOKEN_NUMBER) {
                       char buf[32];
                       snprintf(buf, sizeof(buf), "%lld", (long long)p->current_token.int_val);
                       val = parser_strdup(p, buf);
                       eat(p, TOKEN_NUMBER);
                   } else if (p->current_token.type == TOKEN_IDENTIFIER || p->current_token.type == TOKEN_STRING || p->current_token.type == TOKEN_C_STRING || p->current_token.type == TOKEN_TRUE || p->current_token.type == TOKEN_FALSE) {
                       if (p->current_token.type == TOKEN_TRUE) val = "true";
                       else if (p->current_token.type == TOKEN_FALSE) val = "false";
                       else val = parser_strdup(p, p->current_token.text);
                       eat(p, p->current_token.type);
                   }

                   if (!reason_str) {
                       p->current_token.line = line;
                       p->current_token.col = col;
                       parser_fail(p, "no reason to set setting");
                   }

                   int matched = 0;
#define SET_COMP_BOOL(fname) if (streq_lit(key, #fname)) { p->ctx->settings.fname = (streq_lit(val, "true") || streq_lit(val, "1")); matched = 1; }
#define SET_LEX_BOOL(fname)  if (streq_lit(key, #fname)) { p->l->settings.fname = (streq_lit(val, "true") || streq_lit(val, "1")); matched = 1; }
#define SET_PARS_BOOL(fname) if (streq_lit(key, #fname)) { p->settings.fname = (streq_lit(val, "true") || streq_lit(val, "1")); matched = 1; }
#define SET_COMP_INT(fname) if (streq_lit(key, #fname)) { p->ctx->settings.fname = atoll(val); matched = 1; }

                   if (streq_lit(domain, "compiler")) {
                       if (val) {
                           SET_COMP_BOOL(no_purge);
                           SET_COMP_BOOL(allocator_arc);
                           SET_COMP_BOOL(inject_enum_as_cstring);
                           SET_COMP_BOOL(double_quote_as_string);
                           SET_COMP_INT(big_array_literal_as_flux_emit);
                           SET_COMP_BOOL(resolve_method_call_as_call);
                           if (streq_lit(key, "default_cconv")) {
                               p->ctx->settings.default_cconv = val;
                               matched = 1;
                           }
                       }
                    } else if (streq_lit(domain, "lexer")) {
                        if (val) {
                            SET_LEX_BOOL(require_semicolons);
                            SET_LEX_BOOL(double_quote_as_string);
                            SET_LEX_BOOL(import_require_double_quotes);
                            
                            if (streq_lit(key, "double_quote_as_string")) {
                                p->ctx->settings.double_quote_as_string = p->l->settings.double_quote_as_string; // Sync
                            }
                            
                            if (streq_lit(key, "scope_style")) {
                                if (streq_lit(val, "SCOPE_INDENTATION")) {
                                    p->l->settings.scope_style = SCOPE_INDENTATION;
                                } else if (streq_lit(val, "SCOPE_BRACKETS")) {
                                    p->l->settings.scope_style = SCOPE_BRACKETS;
                                } else {
                                    parser_fail(p, "Unknown scope_style value");
                                }
                                matched = 1;
                            }
                            
                            if (streq_lit(key, "warning_indent_deep")) {
                                p->l->settings.warning_indent_deep = atoi(val);
                                matched = 1;
                            }
                       }
                   } else if (streq_lit(domain, "parser")) {
                       if (val) {
                           SET_PARS_BOOL(require_parens_for_conditions);
                           SET_PARS_BOOL(allow_implicit_return);
                           SET_PARS_BOOL(allow_postfix_types);
                           SET_PARS_BOOL(strict_boolean_conditions);
                           SET_PARS_BOOL(allow_vector_initialization);
                           SET_PARS_BOOL(namespace_auto_search);
                           SET_PARS_BOOL(namespace_ausearch_warning);
                           SET_PARS_BOOL(function_call_require_comma);
                           SET_PARS_BOOL(array_separator_with_space);
                           SET_PARS_BOOL(multiplication_if_digit_word);
                           SET_PARS_BOOL(exponentation_if_word_digit);
                       }
                   }
                   
                    if (!matched) {
                        p->current_token.line = line;
                        p->current_token.col = col;
                        parser_fail(p, "Unknown setting or missing value in premeta block");
                    }
                    
#undef SET_COMP_BOOL
#undef SET_LEX_BOOL
#undef SET_PARS_BOOL
               } else if (p->current_token.type == TOKEN_ASSIGN) {
                   eat(p, TOKEN_ASSIGN);
                   char *val = NULL;
                   if (p->current_token.type == TOKEN_IDENTIFIER || p->current_token.type == TOKEN_NUMBER || p->current_token.type == TOKEN_STRING || p->current_token.type == TOKEN_C_STRING || p->current_token.type == TOKEN_TRUE || p->current_token.type == TOKEN_FALSE) {
                       if (p->current_token.type == TOKEN_TRUE) val = "true";
                       else if (p->current_token.type == TOKEN_FALSE) val = "false";
                       else val = parser_strdup(p, p->current_token.text);
                       eat(p, p->current_token.type);
                   }

                   if (!reason_str) {
                       p->current_token.line = line;
                       p->current_token.col = col;
                       parser_fail(p, "no reason to set setting");
                   }

                   if (streq_lit(domain, "cconv") && val) {
                       p->ctx->settings.default_cconv = val;
                   }
               }
           } else {
               eat(p, p->current_token.type); // skip unhandled
           }
           if (p->current_token.type == TOKEN_SEMICOLON) eat_semi(p);
       }
       eat(p, TOKEN_RBRACE);
       if (p->current_token.type == TOKEN_EOF) return NULL;
       return parse_top_level(p);
   }

  if (p->current_token.type == TOKEN_META || p->current_token.type == TOKEN_POSTMETA) {
      if(modifiers) parser_fail(p, "Modifiers not allowed");
      bool is_post = (p->current_token.type == TOKEN_POSTMETA);
      eat(p, p->current_token.type);

      if (p->current_token.type == TOKEN_LBRACKET) {
          eat(p, TOKEN_LBRACKET);
          while (p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF) { if (p->has_error) break;
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
                          if (streq_lit(key, "cconv")) {
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
          if (p->current_token.type == TOKEN_EOF) return NULL;
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
      (p->current_token.type == TOKEN_OPEN) ||
      (p->current_token.type == TOKEN_CLOSED)) {
    return parse_class_impl(p, modifiers);
  }

  int started_with_union = 0;
  if (p->current_token.type == TOKEN_UNION) {
      if (parser_peek_token(p).type != TOKEN_LBRACKET) {
          return parse_class_impl(p, modifiers);
      }
      started_with_union = 1;
  }

  if (p->current_token.type == TOKEN_LINK) { if(modifiers) parser_fail(p, "Modifiers not allowed"); return parse_link(p); }
  if (p->current_token.type == TOKEN_IMPORT) {
      if (parser_peek_token(p).type == TOKEN_LPAREN) {
          if(modifiers) parser_fail(p, "Modifiers not allowed");
          eat(p, TOKEN_IMPORT);
          eat(p, TOKEN_LPAREN);
          ASTNode *path_expr = parse_expression(p);
          eat(p, TOKEN_RPAREN);
          ImportExprNode *ie = parser_alloc(p, sizeof(ImportExprNode));
          ie->base.type = NODE_IMPORT_EXPR;
          ie->path = NULL;
          if (path_expr && path_expr->type == NODE_LITERAL && ((LiteralNode*)path_expr)->var_type.base == TYPE_CHAR && ((LiteralNode*)path_expr)->var_type.ptr_depth == 1) {
              ie->path = parser_strdup(p, ((LiteralNode*)path_expr)->val.str_val);
          }
          return (ASTNode*)ie;
      }
      if(modifiers) { parser_fail(p, "Modifiers not allowed"); }
      return parse_import(p);
  }
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

  char *saved_type_name = NULL;
  if (p->current_token.type == TOKEN_IDENTIFIER) {
      saved_type_name = parser_strdup(p, p->current_token.text);
  }

  VarType vtype = parse_type(p);
  modifiers |= parse_modifiers(p);
  if (vtype.base == TYPE_UNKNOWN) {
      if (modifiers) parser_fail(p, "Modifiers not allowed on statement");
      return parse_single_statement_or_block(p);
  }

  if (vtype.base == TYPE_ENUM && saved_type_name && p->current_token.type != TOKEN_IDENTIFIER) {
      VarRefNode *vn = parser_alloc(p, sizeof(VarRefNode));
      vn->base.type = NODE_VAR_REF;
      vn->name = saved_type_name;
      return (ASTNode*)vn;
  }

  if (started_with_union && p->current_token.type == TOKEN_IDENTIFIER && parser_peek_token(p).type == TOKEN_SEMICOLON) {
      char *name = parser_strdup(p, p->current_token.text);
      eat(p, TOKEN_IDENTIFIER);
      eat_semi(p);
      register_alias(p, name, vtype);
      register_typename(p, name, 0);
      return NULL;
  }

  if (vtype.base == TYPE_CLASS && p->current_token.type == TOKEN_LPAREN) {
      if (modifiers) parser_fail(p, "Invalid modifier here");
      VarRefNode *vn = parser_alloc(p, sizeof(VarRefNode));
      vn->base.type = NODE_VAR_REF;
      vn->name = vtype.class_name;
      ASTNode* call = parse_call(p, (ASTNode*)vn);
      call = parse_postfix(p, call);
      eat_semi(p);
      set_loc(call, line, col);
      return call;
  }

  if (p->current_token.type == TOKEN_LPAREN) {
      char *name = NULL;
      vtype = parse_func_ptr_decl(p, vtype, &name);

      ASTNode *init = parse_initializer(p, vtype);
      eat_semi(p);

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

  return parse_func_def_after_type(p, modifiers, vtype, line, col, is_flux);
}

// Parses a function definition given an already-parsed return type.
// Used both for normal function definitions and for functions decorated
// with an attached `errnum [...]` error set.
ASTNode* parse_func_def_after_type(Parser *p, int modifiers, VarType vtype, int line, int col, int is_flux) {
  char *name = NULL;
  if (p->current_token.type == TOKEN_PREFOP || p->current_token.type == TOKEN_INFOP || p->current_token.type == TOKEN_SUFFOP ||
      p->current_token.type == TOKEN_PREMUT || p->current_token.type == TOKEN_INFMUT || p->current_token.type == TOKEN_SUFMUT) {
      int kind = p->current_token.type;
      eat(p, kind);
      eat(p, TOKEN_LBRACKET);
      TokenType op_type = p->current_token.type;
      char name_buf[64];
      snprintf(name_buf, sizeof(name_buf), "__op_%d_%d", kind, op_type);
      name = parser_strdup(p, name_buf);
      eat(p, op_type);
      eat(p, TOKEN_RBRACKET);
  } else {
      if (p->current_token.type != TOKEN_IDENTIFIER) { parser_fail(p, "Expected identifier definition after type"); }
      name = parser_strdup(p, p->current_token.text);
      p->current_token.text = NULL; eat(p, TOKEN_IDENTIFIER);
  }

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
    int is_varargs = 0;
    Parameter *params_head = NULL; Parameter **curr_param = &params_head;
    if (p->current_token.type != TOKEN_RPAREN) {
      while (1) { if (p->has_error) break;
        if (p->current_token.type == TOKEN_ELLIPSIS) { eat(p, TOKEN_ELLIPSIS); is_varargs = 1; break; }
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

        Parameter *pm = parser_alloc_raw(p, sizeof(Parameter));
        apply_param_modifiers(pm, pmods);
        pm->type = ptype; pm->name = pname;

        if (p->current_token.type == TOKEN_ASSIGN) {
            eat(p, TOKEN_ASSIGN);
            pm->default_value = parse_expression(p);
        } else {
            pm->default_value = NULL;
        }

        *curr_param = pm; curr_param = &pm->next;
        if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA); else break;
      }
    }
    eat(p, TOKEN_RPAREN); eat(p, TOKEN_LBRACE);
    ASTNode *body = parse_statements(p); eat(p, TOKEN_RBRACE);

    apply_implicit_return(p, &body);

    FuncDefNode *node = parser_alloc(p, sizeof(FuncDefNode));
    node->base.type = NODE_FUNC_DEF; node->name = name; node->ret_type = vtype; node->params = params_head; node->body = body;
    node->has_body = 1;
    node->is_flux = is_flux;
    node->is_varargs = is_varargs;
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

    while (1) { if (p->has_error) break;
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
        if (!init && current_vtype.base == TYPE_AUTO) { parser_fail(p, "'let' variable declaration must have an initializer"); }

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
}
