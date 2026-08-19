/**
 * @file expr.c
 * @brief Expression parsing implementation for the Alkyl parser.
 */
#include "parser_internal.h"
#include <string.h>
#include <stdlib.h>

Token parser_peek_token_n(Parser *p, int offset);

ASTNode* parse_unary(Parser *p);

ASTNode* parse_call(Parser *p, ASTNode *target) {
  char *name = NULL;
  if (target && target->type == NODE_VAR_REF) {
      name = ((VarRefNode*)target)->name;
  }

  eat(p, TOKEN_LPAREN);
  ASTNode *args_head = NULL;
  ASTNode **curr_arg = &args_head;
  if (p->current_token.type != TOKEN_RPAREN) {
    ASTNode *expr = parse_expression(p);
    if (expr && expr->type == NODE_ASSIGN && ((AssignNode*)expr)->op == TOKEN_ASSIGN && ((AssignNode*)expr)->name != NULL) {
        NamedArgNode *narg = parser_alloc(p, sizeof(NamedArgNode));
        narg->base.type = NODE_NAMED_ARG;
        narg->name = ((AssignNode*)expr)->name;
        narg->value = ((AssignNode*)expr)->value;
        narg->base.line = expr->line; narg->base.col = expr->col;
        expr = (ASTNode*)narg;
    }
    if (!expr) { p->has_error = 1; return NULL; }
    *curr_arg = expr;
    curr_arg = &(*curr_arg)->next;
    while (p->current_token.type != TOKEN_RPAREN && p->current_token.type != TOKEN_EOF) { if (p->has_error) break;
      if (p->current_token.type == TOKEN_COMMA) {
          eat(p, TOKEN_COMMA);
      } else {
          if (p->settings.function_call_require_comma) break;
      }
      expr = parse_expression(p);
      if (expr && expr->type == NODE_ASSIGN && ((AssignNode*)expr)->op == TOKEN_ASSIGN && ((AssignNode*)expr)->name != NULL) {
          NamedArgNode *narg = parser_alloc(p, sizeof(NamedArgNode));
          narg->base.type = NODE_NAMED_ARG;
          narg->name = ((AssignNode*)expr)->name;
          narg->value = ((AssignNode*)expr)->value;
          narg->base.line = expr->line; narg->base.col = expr->col;
          expr = (ASTNode*)narg;
      }
      if (!expr) { p->has_error = 1; break; }
      *curr_arg = expr;
      curr_arg = &(*curr_arg)->next;
    }
  }
  eat(p, TOKEN_RPAREN);

  if (target && target->type == NODE_MEMBER_ACCESS) {
    MemberAccessNode *ma = (MemberAccessNode*)target;
    MethodCallNode *mc = parser_alloc(p, sizeof(MethodCallNode));
    mc->base.type = NODE_METHOD_CALL;
    mc->object = ma->object;
    mc->method_name = ma->member_name;
    mc->args = args_head;
    mc->mangled_name = NULL;
    mc->owner_class = NULL;
    mc->is_static = 0;
    debug_parser("Created MethodCall for member '%s' line=%d col=%d\n", mc->method_name, mc->base.line, mc->base.col);
    return (ASTNode*)mc;
  }

  CallNode *node = parser_alloc(p, sizeof(MethodCallNode));
  node->base.type = NODE_CALL;
  node->name = name;
  node->target = target;
  node->args = args_head;
  debug_parser("Created Call name=%s target_type=%d line=%d col=%d node=%p target=%p\n", node->name ? node->name : "(null)", target ? (int)target->type : -1, node->base.line, node->base.col, (void*)node, (void*)target);
  return (ASTNode*)node;
}

static int is_unambiguous_expr_start(Parser *p) {
    TokenType t = p->current_token.type;
    return t == TOKEN_IDENTIFIER || t == TOKEN_NUMBER || t == TOKEN_UINT_LIT || t == TOKEN_LONG_LIT ||
           t == TOKEN_ULONG_LIT || t == TOKEN_LONG_LONG_LIT || t == TOKEN_ULONG_LONG_LIT ||
           t == TOKEN_SINGLE_LIT || t == TOKEN_DOUBLE_LIT || t == TOKEN_LONG_DOUBLE_LIT ||
           t == TOKEN_STRING || t == TOKEN_C_STRING || t == TOKEN_BYTE_STRING ||
           t == TOKEN_TRUE || t == TOKEN_FALSE ||
           t == TOKEN_CHAR_LIT || t == TOKEN_LPAREN || t == TOKEN_LBRACKET ||
           t == TOKEN_TYPEOF || t == TOKEN_KW_SIZEOF || t == TOKEN_KW_ALIGNOF ||
           t == TOKEN_KW_DEFINED || t == TOKEN_HASMETHOD || t == TOKEN_HASATTRIBUTE ||
           t == TOKEN_NOT || t == TOKEN_BIT_NOT || t == TOKEN_IMPORT;
}

static ASTNode* parse_space_separated_call(Parser *p, ASTNode *target) {
  ASTNode *args_head = NULL;
  ASTNode **curr_arg = &args_head;

  int last_line = target ? target->line : p->current_token.line;

  while (1) {
    if (p->current_token.type == TOKEN_SEMICOLON ||
        p->current_token.type == TOKEN_RPAREN ||
        p->current_token.type == TOKEN_RBRACKET ||
        p->current_token.type == TOKEN_RBRACE ||
        p->current_token.type == TOKEN_ELSE ||
        p->current_token.type == TOKEN_EOF) {
        break;
    }

    if (p->current_token.line > last_line) {
        break;
    }

    if (!is_unambiguous_expr_start(p)) {
        break;
    }

    if (!p->settings.greedy_space_calls) p->in_space_separated_call++;
    ASTNode *expr = parse_unary(p);
    if (!p->settings.greedy_space_calls) p->in_space_separated_call--;

    if (!expr) break;

    last_line = expr->line;

    *curr_arg = expr;
    curr_arg = &(*curr_arg)->next;

    if (p->current_token.type == TOKEN_COMMA) {
      eat(p, TOKEN_COMMA);
    } else {
      if (p->settings.function_call_require_comma) break;
    }
  }

  if (target && target->type == NODE_MEMBER_ACCESS) {
    MemberAccessNode *ma = (MemberAccessNode*)target;
    MethodCallNode *mc = parser_alloc(p, sizeof(MethodCallNode));
    mc->base.type = NODE_METHOD_CALL;
    mc->object = ma->object;
    mc->method_name = ma->member_name;
    mc->args = args_head;
    mc->mangled_name = NULL;
    mc->owner_class = NULL;
    mc->is_static = 0;
    return (ASTNode*)mc;
  }

  CallNode *node = parser_alloc(p, sizeof(MethodCallNode));
  node->base.type = NODE_CALL;
  char *name = NULL;
  if (target && target->type == NODE_VAR_REF) {
      name = ((VarRefNode*)target)->name;
  }
  node->name = name;
  node->target = target;
  node->args = args_head;
  return (ASTNode*)node;
}

ASTNode* parse_postfix(Parser *p, ASTNode *node) {
    while (1) { if (p->has_error) break;
        int line = p->current_token.line;
        int col = p->current_token.col;

        if (p->current_token.type == TOKEN_DOT) {
            eat(p, TOKEN_DOT);
            if (p->current_token.type != TOKEN_IDENTIFIER) parser_fail(p, "Expected member name after '.'");
            char *member = p->current_token.text;
            p->current_token.text = NULL;
            eat(p, TOKEN_IDENTIFIER);

            if (p->current_token.type == TOKEN_LPAREN) {
                eat(p, TOKEN_LPAREN);
                ASTNode *args_head = NULL;
                ASTNode **curr_arg = &args_head;
                if (p->current_token.type != TOKEN_RPAREN) {
                    ASTNode *expr = parse_expression(p);
                    if (expr && expr->type == NODE_ASSIGN && ((AssignNode*)expr)->op == TOKEN_ASSIGN && ((AssignNode*)expr)->name != NULL) {
                        NamedArgNode *narg = parser_alloc(p, sizeof(NamedArgNode));
                        narg->base.type = NODE_NAMED_ARG;
                        narg->name = ((AssignNode*)expr)->name;
                        narg->value = ((AssignNode*)expr)->value;
                        narg->base.line = expr->line; narg->base.col = expr->col;
                        expr = (ASTNode*)narg;
                    }
                    if (!expr) { p->has_error = 1; }
                    else {
                        *curr_arg = expr;
                        curr_arg = &(*curr_arg)->next;
                    }
                    while (p->current_token.type != TOKEN_RPAREN && p->current_token.type != TOKEN_EOF) { if (p->has_error) break;
                        if (p->current_token.type == TOKEN_COMMA) {
                            eat(p, TOKEN_COMMA);
                        } else {
                            if (p->settings.function_call_require_comma) break;
                        }
                        expr = parse_expression(p);
                        if (expr && expr->type == NODE_ASSIGN && ((AssignNode*)expr)->op == TOKEN_ASSIGN && ((AssignNode*)expr)->name != NULL) {
                            NamedArgNode *narg = parser_alloc(p, sizeof(NamedArgNode));
                            narg->base.type = NODE_NAMED_ARG;
                            narg->name = ((AssignNode*)expr)->name;
                            narg->value = ((AssignNode*)expr)->value;
                            narg->base.line = expr->line; narg->base.col = expr->col;
                            expr = (ASTNode*)narg;
                        }
                        if (!expr) { p->has_error = 1; break; }
                        *curr_arg = expr;
                        curr_arg = &(*curr_arg)->next;
                    }
                }
                eat(p, TOKEN_RPAREN);

                MethodCallNode *mc = parser_alloc(p, sizeof(MethodCallNode));
                mc->base.type = NODE_METHOD_CALL;
                mc->object = node;
                mc->method_name = member;
                mc->args = args_head;
                node = (ASTNode*)mc;
            } else {
                MemberAccessNode *ma = parser_alloc(p, sizeof(MemberAccessNode));
                ma->base.type = NODE_MEMBER_ACCESS;
                ma->object = node;
                ma->member_name = member;
                node = (ASTNode*)ma;
            }
            set_loc(node, line, col);
        }
        else if ((p->current_token.type == TOKEN_LBRACKET || p->current_token.type == TOKEN_LT) && p->disable_space_call == 0 && (!p->current_token.has_space_before || p->in_space_separated_call > 0)) {
            int is_lt = (p->current_token.type == TOKEN_LT);
            int saved_pos = p->token_pos;
            Token saved_tok = p->current_token;

            eat(p, p->current_token.type);

            if (is_lt && !is_type_start(p)) {
                p->token_pos = saved_pos;
                p->current_token = saved_tok;
                break;
            }

            TokenType end_token = is_lt ? TOKEN_GT : TOKEN_RBRACKET;

            if (is_type_start(p)) {
                int max_args = 16;
                VarType *types = parser_alloc_raw(p, sizeof(VarType) * max_args);
                int num_types = 0;

                while (p->current_token.type != end_token) { if (p->has_error) break;
                    if (num_types >= max_args) {
                        report_error(p->l, p->current_token, "Too many template type parameters");
                        break;
                    }
                    types[num_types++] = parse_type(p);
                    if (p->current_token.type == TOKEN_COMMA) {
                        eat(p, TOKEN_COMMA);
                    } else {
                        break;
                    }
                }
                eat(p, end_token);

                TemplateInstNode *ti = parser_alloc(p, sizeof(TemplateInstNode));
                ti->base.type = NODE_TEMPLATE_INSTANTIATION;
                ti->target = node;
                ti->template_types = types;
                ti->num_template_types = num_types;
                node = (ASTNode*)ti;
            } else {
                ASTNode *index = parse_expression(p);
                eat(p, end_token);

                IndexAccessNode *aa = parser_alloc(p, sizeof(IndexAccessNode));
                aa->base.type = NODE_INDEX_ACCESS;
                aa->target = node;
                aa->index = index;
                node = (ASTNode*)aa;
            }
            set_loc(node, line, col);
        }
        else if (p->current_token.type == TOKEN_INCREMENT || p->current_token.type == TOKEN_DECREMENT) {
            int op = p->current_token.type;
            eat(p, op);
            IncDecNode *id = parser_alloc(p, sizeof(IncDecNode));
            id->base.type = NODE_INC_DEC;
            id->target = node;
            id->is_prefix = 0;
            id->op = op;
            node = (ASTNode*)id;
            set_loc(node, line, col);
        }
        else if (p->current_token.type == TOKEN_AS) {
            eat(p, TOKEN_AS);
            VarType t = parse_type(p);

            CastNode *cn = parser_alloc(p, sizeof(CastNode));
            cn->base.type = NODE_CAST;
            cn->operand = node;
            cn->var_type = t;
            node = (ASTNode*)cn;
            set_loc(node, line, col);
        }
        else if (p->current_token.type == TOKEN_BEING) {
            eat(p, TOKEN_BEING);
            Endianness endian = ENDIAN_NATIVE;
            if (p->current_token.type == TOKEN_LBRACKET) {
                eat(p, TOKEN_LBRACKET);
                if (p->current_token.type == TOKEN_IDENTIFIER) {
                    if (streq_lit(p->current_token.text, "little")) {
                        endian = ENDIAN_LITTLE;
                    } else if (streq_lit(p->current_token.text, "big")) {
                        endian = ENDIAN_BIG;
                    } else {
                        parser_fail(p, "Invalid endianness specifier for being, expected \"little\" or \"big\"");
                    }
                    eat(p, TOKEN_IDENTIFIER);
                } else {
                    parser_fail(p, "Expected endianness specifier \"little\" or \"big\"");
                }
                eat(p, TOKEN_RBRACKET);
            }
            VarType t = parse_type(p);

            BeingNode *bn = parser_alloc(p, sizeof(BeingNode));
            bn->base.type = NODE_BEING;
            bn->operand = node;
            bn->var_type = t;
            bn->endian = endian;
            node = (ASTNode*)bn;
            set_loc(node, line, col);
        }
        else if (p->current_token.type == TOKEN_LPAREN) {
            debug_parser("parse_postfix: before parse_call, node->type=%d\n", node ? (int)node->type : -1);
            node = parse_call(p, node);
            set_loc(node, line, col);
        }
        else if ((p->current_token.type == TOKEN_STRING || p->current_token.type == TOKEN_C_STRING) && p->in_space_separated_call == 0 && p->disable_space_call == 0) {
            if (node && p->current_token.line > node->line) break;
            node = parse_space_separated_call(p, node);
            set_loc(node, line, col);
        }
        else if (p->in_space_separated_call == 0 && p->disable_space_call == 0 && is_unambiguous_expr_start(p)) {
            if (node && p->current_token.line > node->line) break;
            node = parse_space_separated_call(p, node);
            set_loc(node, line, col);
        }
        // parse other postfix
        else {
            break;
        }
    }
    return node;
}

// TODO split this
ASTNode* parse_factor(Parser *p) {
  ASTNode *node = NULL;
  int line = p->current_token.line;
  int col = p->current_token.col;

    if (p->current_token.type == TOKEN_KW_INT || p->current_token.type == TOKEN_KW_LONG ||
      p->current_token.type == TOKEN_KW_CHAR || p->current_token.type == TOKEN_KW_SINGLE ||
      p->current_token.type == TOKEN_KW_DOUBLE || p->current_token.type == TOKEN_KW_VOID ||
      p->current_token.type == TOKEN_KW_BOOL || p->current_token.type == TOKEN_KW_UNSIGNED ||
      p->current_token.type == TOKEN_KW_SHORT) {

      VarType t = parse_type(p);

      while (p->current_token.type == TOKEN_LBRACKET) {
          eat(p, TOKEN_LBRACKET);
          if (p->current_token.type == TOKEN_RBRACKET) {
              eat(p, TOKEN_RBRACKET);
              t.array_size = 1;
          } else {
              // This is a fixed size array, skip expressions for now
              while (p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF) {
                  eat(p, p->current_token.type);
              }
              eat(p, TOKEN_RBRACKET);
              t.array_size = 1; // Just a marker
          }
      }

      SizeOfNode *sn = parser_alloc(p, sizeof(SizeOfNode));
      sn->base.type = NODE_TYPEOF;
      sn->target_type = t;
      sn->operand = NULL;
      node = (ASTNode*)sn;
      set_loc(node, line, col);
      return node;
  }

  if (p->current_token.type == TOKEN_TYPEOF) {
      eat(p, TOKEN_TYPEOF);
      int has_paren = (p->current_token.type == TOKEN_LPAREN);
      if (has_paren) eat(p, TOKEN_LPAREN);
      if (is_type_start(p)) {
          VarType t = parse_type(p);
          while (p->current_token.type == TOKEN_LBRACKET) {
              eat(p, TOKEN_LBRACKET);
              if (p->current_token.type == TOKEN_RBRACKET) {
                  eat(p, TOKEN_RBRACKET);
                  t.array_size = 1;
              } else {
                  while (p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF) {
                      eat(p, p->current_token.type);
                  }
                  eat(p, TOKEN_RBRACKET);
                  t.array_size = 1;
              }
          }
          // If typeof takes a type, we wrap it in a CastNode or a new TypeOfNode.
          // Since TypeOfNode is currently UnaryOpNode, it only takes an operand expression.
          // For now we'll put a dummy literal here, but a real TypeOfTypeNode would be better.
          SizeOfNode *sn = parser_alloc(p, sizeof(SizeOfNode));
          sn->base.type = NODE_TYPEOF;
          sn->target_type = t;
          node = (ASTNode*)sn;
      } else {
          SizeOfNode *sn2 = parser_alloc(p, sizeof(SizeOfNode));
          sn2->base.type = NODE_TYPEOF;
          sn2->target_type.base = TYPE_UNKNOWN;
          sn2->operand = parse_expression(p);
          node = (ASTNode*)sn2;
      }
      if (has_paren) eat(p, TOKEN_RPAREN);
      set_loc(node, line, col);
  }
  else if (p->current_token.type == TOKEN_KW_DEFINED) {
      p->disable_macro_expansion = 1;
      eat(p, TOKEN_KW_DEFINED);

      ASTNode *expr;
      if (p->current_token.type == TOKEN_LPAREN) {
          eat(p, TOKEN_LPAREN);
          expr = parse_expression(p);
          p->disable_macro_expansion = 0;
          eat(p, TOKEN_RPAREN);
      } else {
          expr = parse_unary(p);
          p->disable_macro_expansion = 0;
      }
      UnaryOpNode *u = parser_alloc(p, sizeof(UnaryOpNode));
      u->base.type = NODE_DEFINED;
      u->operand = expr;
      node = (ASTNode*)u;
      set_loc(node, line, col);
  }
  else if (p->current_token.type == TOKEN_HASMETHOD) {
      eat(p, TOKEN_HASMETHOD);
      ASTNode *expr;
      if (p->current_token.type == TOKEN_LPAREN) {
          eat(p, TOKEN_LPAREN);
          expr = parse_expression(p);
          eat(p, TOKEN_RPAREN);
      } else {
          expr = parse_unary(p);
      }
      UnaryOpNode *u = parser_alloc(p, sizeof(UnaryOpNode));
      u->base.type = NODE_HAS_METHOD;
      u->operand = expr;
      node = (ASTNode*)u;
      set_loc(node, line, col);
  }
  else if (p->current_token.type == TOKEN_HASATTRIBUTE) {
      eat(p, TOKEN_HASATTRIBUTE);
      ASTNode *expr;
      if (p->current_token.type == TOKEN_LPAREN) {
          eat(p, TOKEN_LPAREN);
          expr = parse_expression(p);
          eat(p, TOKEN_RPAREN);
      } else {
          expr = parse_unary(p);
      }
      UnaryOpNode *u = parser_alloc(p, sizeof(UnaryOpNode));
      u->base.type = NODE_HAS_ATTRIBUTE;
      u->operand = expr;
      node = (ASTNode*)u;
      set_loc(node, line, col);
  }
  else if (p->current_token.type == TOKEN_AT) {
      Token after_at = parser_peek_token(p);
      if (after_at.type == TOKEN_IDENTIFIER && streq_lit(after_at.text, "c")) {
          Token after_c = parser_peek_token_n(p, 1);
          if (after_c.type == TOKEN_IMPORT) {
              eat(p, TOKEN_AT);
              eat(p, TOKEN_IDENTIFIER);
              eat(p, TOKEN_IMPORT);
              eat(p, TOKEN_LPAREN);
              ASTNode *path_expr = parse_expression(p);
              eat(p, TOKEN_RPAREN);
              ImportExprNode *ie = parser_alloc(p, sizeof(ImportExprNode));
              ie->base.type = NODE_IMPORT_EXPR;
              ie->path = NULL;
              ie->header = HEADER_C;
              if (path_expr && path_expr->type == NODE_LITERAL && ((LiteralNode*)path_expr)->var_type.base == TYPE_CHAR && ((LiteralNode*)path_expr)->var_type.ptr_depth == 1) {
                  ie->path = parser_strdup(p, ((LiteralNode*)path_expr)->val.str_val);
              }
              node = (ASTNode*)ie;
              set_loc(node, line, col);
          }
      }
  }
  else if (p->current_token.type == TOKEN_IMPORT) {
      eat(p, TOKEN_IMPORT);
      ImportExprNode *ie = parser_alloc(p, sizeof(ImportExprNode));
      ie->base.type = NODE_IMPORT_EXPR;
      ie->path = NULL;
      ie->header = HEADER_ALKYL;
      if (p->current_token.type == TOKEN_LPAREN) {
          eat(p, TOKEN_LPAREN);
          if (p->current_token.type == TOKEN_STRING || p->current_token.type == TOKEN_C_STRING) {
              ie->path = parser_strdup(p, p->current_token.text);
              p->current_token.text = NULL;
              eat(p, p->current_token.type);
          } else {
              ASTNode *path_expr = parse_expression(p);
              if (path_expr && path_expr->type == NODE_LITERAL && ((LiteralNode*)path_expr)->var_type.base == TYPE_CHAR && ((LiteralNode*)path_expr)->var_type.ptr_depth == 1) {
                  ie->path = parser_strdup(p, ((LiteralNode*)path_expr)->val.str_val);
              }
          }
          eat(p, TOKEN_RPAREN);
      } else if (p->current_token.type == TOKEN_STRING || p->current_token.type == TOKEN_C_STRING) {
          ie->path = parser_strdup(p, p->current_token.text);
          p->current_token.text = NULL;
          eat(p, p->current_token.type);
      } else {
          char path_buf[256] = {0};
          size_t path_len = 0;
          while (p->current_token.type == TOKEN_IDENTIFIER ||
                 p->current_token.type == TOKEN_DOT ||
                 p->current_token.type == TOKEN_SLASH) {
              if (p->current_token.type == TOKEN_DOT || p->current_token.type == TOKEN_SLASH) {
                  if (path_len + 1 < sizeof(path_buf)) {
                      path_buf[path_len++] = '/';
                      path_buf[path_len] = '\0';
                  }
              } else {
                  if (p->current_token.text) {
                      size_t text_len = strlen(p->current_token.text);
                      if (text_len + path_len < sizeof(path_buf) - 1) {
                          memcpy(path_buf + path_len, p->current_token.text, text_len);
                          path_len += text_len;
                          path_buf[path_len] = '\0';
                      }
                  }
              }
              eat(p, p->current_token.type);
          }
          if (path_len > 0) {
              ie->path = parser_strdup(p, path_buf);
          }
      }
      node = (ASTNode*)ie;
      set_loc(node, line, col);
  }
  else if (p->current_token.type == TOKEN_LBRACKET) {
    eat(p, TOKEN_LBRACKET);
    ASTNode *elems_head = NULL;
    ASTNode **curr_elem = &elems_head;

    int restore_space_call = 0;
    if (p->settings.array_separator_with_space) {
        p->disable_space_call++;
        restore_space_call = 1;
    }

    if (p->current_token.type != TOKEN_RBRACKET) {
      *curr_elem = parse_expression(p);

      if ((p->current_token.type >= TOKEN_RANGE_INCL && p->current_token.type <= TOKEN_RANGE_INCL_LTE) || p->current_token.type == TOKEN_RANGE) {
          TokenType range_type = p->current_token.type;
          eat(p, range_type);
          ASTNode *end_expr = parse_expression(p);

          if ((*curr_elem)->type == NODE_LITERAL && end_expr->type == NODE_LITERAL) {
              LiteralNode *start_lit = (LiteralNode*)*curr_elem;
              LiteralNode *end_lit = (LiteralNode*)end_expr;
              if (start_lit->var_type.base == TYPE_INT && end_lit->var_type.base == TYPE_INT) {
                  int start_val = start_lit->val.int_val;
                  int end_val = end_lit->val.int_val;
                  int step = (start_val <= end_val) ? 1 : -1;

                  if (range_type == TOKEN_RANGE_EXCL || range_type == TOKEN_RANGE_EXCL_GT || range_type == TOKEN_RANGE) {
                      if (start_val != end_val) {
                          end_val -= step;
                      } else {
                          // start == end, exclusive means empty range. For simplicity, just make step such that loop doesn't run, or make start_val > end_val
                          step = 1; start_val = 1; end_val = 0; // empty range
                          *curr_elem = NULL; // Removing the first element (wait, this might break if it's the only element, but handled below)
                      }
                  }

                  if (step > 0 ? (start_val <= end_val) : (start_val >= end_val)) {
                      long long count = (step > 0 ? ((long long)end_val - start_val) : ((long long)start_val - end_val)) / (step > 0 ? step : -step) + 1;
                      int make_flux = 0;
                      if (p->ctx->settings.big_array_literal_as_flux_emit != -1) {
                          if (p->ctx->settings.big_array_literal_as_flux_emit == 0 || count > p->ctx->settings.big_array_literal_as_flux_emit) {
                              make_flux = 1;
                          }
                      }

                      if (make_flux) {
                          static int synthetic_flux_counter = 0;
                          char flux_name[64];
                          snprintf(flux_name, sizeof(flux_name), "__synthetic_array_flux_%d", synthetic_flux_counter++);

                          FuncDefNode *fn = parser_alloc(p, sizeof(FuncDefNode));
                          fn->base.type = NODE_FUNC_DEF;
                          fn->name = parser_strdup(p, flux_name);
                          fn->is_flux = 1;
                          fn->ret_type = start_lit->var_type;
                          fn->has_body = 1;

                          VarDeclNode *decl = parser_alloc(p, sizeof(VarDeclNode));
                          decl->base.type = NODE_VAR_DECL;
                          decl->name = parser_strdup(p, "i");
                          decl->var_type = start_lit->var_type;
                          LiteralNode *start_node = parser_alloc(p, sizeof(LiteralNode));
                          start_node->base.type = NODE_LITERAL;
                          start_node->var_type = start_lit->var_type;
                          start_node->val.int_val = start_val;
                          decl->initializer = (ASTNode*)start_node;

                          WhileNode *wh = parser_alloc(p, sizeof(WhileNode));
                          wh->base.type = NODE_WHILE;

                          BinaryOpNode *cond = parser_alloc(p, sizeof(BinaryOpNode));
                          cond->base.type = NODE_BINARY_OP;
                          cond->op = (step > 0) ? TOKEN_LTE : TOKEN_GTE;
                          VarRefNode *ref_i = parser_alloc(p, sizeof(VarRefNode));
                          ref_i->base.type = NODE_VAR_REF;
                          ref_i->name = parser_strdup(p, "i");
                          cond->left = (ASTNode*)ref_i;
                          LiteralNode *end_node = parser_alloc(p, sizeof(LiteralNode));
                          end_node->base.type = NODE_LITERAL;
                          end_node->var_type = start_lit->var_type;
                          end_node->val.int_val = end_val;
                          cond->right = (ASTNode*)end_node;
                          wh->condition = (ASTNode*)cond;

                          EmitNode *em = parser_alloc(p, sizeof(EmitNode));
                          em->base.type = NODE_EMIT;
                          VarRefNode *ref_i2 = parser_alloc(p, sizeof(VarRefNode));
                          ref_i2->base.type = NODE_VAR_REF;
                          ref_i2->name = parser_strdup(p, "i");
                          em->value = (ASTNode*)ref_i2;

                          AssignNode *inc = parser_alloc(p, sizeof(AssignNode));
                          inc->base.type = NODE_ASSIGN;
                          inc->op = TOKEN_PLUS_ASSIGN;
                          VarRefNode *ref_i3 = parser_alloc(p, sizeof(VarRefNode));
                          ref_i3->base.type = NODE_VAR_REF;
                          ref_i3->name = parser_strdup(p, "i");
                          inc->target = (ASTNode*)ref_i3;
                          LiteralNode *step_node = parser_alloc(p, sizeof(LiteralNode));
                          step_node->base.type = NODE_LITERAL;
                          step_node->var_type = start_lit->var_type;
                          step_node->val.int_val = step;
                          inc->value = (ASTNode*)step_node;

                          em->base.next = (ASTNode*)inc;
                          wh->body = (ASTNode*)em;

                          decl->base.next = (ASTNode*)wh;
                          fn->body = (ASTNode*)decl;

                          fn->base.next = p->synthetic_classes;
                          p->synthetic_classes = (ASTNode*)fn;

                          CallNode *call = parser_alloc(p, sizeof(CallNode));
                          call->base.type = NODE_CALL;
                          call->name = parser_strdup(p, flux_name);
                          call->target = NULL;
                          call->args = NULL;

                          *curr_elem = (ASTNode*)call;
                          curr_elem = &(*curr_elem)->next;
                      } else {
                          start_lit->val.int_val = start_val;
                          int i = start_val + step;
                          while (step > 0 ? (i <= end_val) : (i >= end_val)) {
                              LiteralNode *new_lit = parser_alloc(p, sizeof(LiteralNode));
                              new_lit->base.type = NODE_LITERAL;
                              new_lit->var_type = start_lit->var_type;
                              new_lit->val.int_val = i;
                              (*curr_elem)->next = (ASTNode*)new_lit;
                              curr_elem = &(*curr_elem)->next;
                              i += step;
                          }
                          curr_elem = &(*curr_elem)->next; // point to next of the last element
                      }
                  } else {
                      *curr_elem = NULL; // empty
                  }
              } else {
                  parser_fail(p, "Range bounds must be integers");
              }
          } else {
              parser_fail(p, "Range bounds in array literals must be compile-time literals for now");
          }
      } else {
          curr_elem = &(*curr_elem)->next;
      }

      while (p->current_token.type == TOKEN_COMMA || (p->settings.array_separator_with_space && p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF)) {
        if (p->has_error) break;
        if (p->current_token.type == TOKEN_COMMA) {
            eat(p, TOKEN_COMMA);
        }
        if (p->current_token.type == TOKEN_RBRACKET) break;

        *curr_elem = parse_expression(p);

        if ((p->current_token.type >= TOKEN_RANGE_INCL && p->current_token.type <= TOKEN_RANGE_INCL_LTE) || p->current_token.type == TOKEN_RANGE) {
            TokenType range_type = p->current_token.type;
            eat(p, range_type);
            ASTNode *end_expr = parse_expression(p);
            if ((*curr_elem)->type == NODE_LITERAL && end_expr->type == NODE_LITERAL) {
                LiteralNode *start_lit = (LiteralNode*)*curr_elem;
                LiteralNode *end_lit = (LiteralNode*)end_expr;
                if (start_lit->var_type.base == TYPE_INT && end_lit->var_type.base == TYPE_INT) {
                    int start_val = start_lit->val.int_val;
                    int end_val = end_lit->val.int_val;
                    int step = (start_val <= end_val) ? 1 : -1;
                    if (range_type == TOKEN_RANGE_EXCL || range_type == TOKEN_RANGE_EXCL_GT || range_type == TOKEN_RANGE) {
                        if (start_val != end_val) end_val -= step;
                        else { step = 1; start_val = 1; end_val = 0; *curr_elem = NULL; }
                    }
                    if (step > 0 ? (start_val <= end_val) : (start_val >= end_val)) {
                        long long count = (step > 0 ? ((long long)end_val - start_val) : ((long long)start_val - end_val)) / (step > 0 ? step : -step) + 1;
                        int make_flux = 0;
                        if (p->ctx->settings.big_array_literal_as_flux_emit != -1) {
                            if (p->ctx->settings.big_array_literal_as_flux_emit == 0 || count > p->ctx->settings.big_array_literal_as_flux_emit) {
                                make_flux = 1;
                            }
                        }

                        if (make_flux) {
                            static int synthetic_flux_counter = 0;
                            char flux_name[64];
                            snprintf(flux_name, sizeof(flux_name), "__synthetic_array_flux_%d", synthetic_flux_counter++);

                            FuncDefNode *fn = parser_alloc(p, sizeof(FuncDefNode));
                            fn->base.type = NODE_FUNC_DEF;
                            fn->name = parser_strdup(p, flux_name);
                            fn->is_flux = 1;
                            fn->ret_type = start_lit->var_type;
                            fn->has_body = 1;

                            VarDeclNode *decl = parser_alloc(p, sizeof(VarDeclNode));
                            decl->base.type = NODE_VAR_DECL;
                            decl->name = parser_strdup(p, "i");
                            decl->var_type = start_lit->var_type;
                            LiteralNode *start_node = parser_alloc(p, sizeof(LiteralNode));
                            start_node->base.type = NODE_LITERAL;
                            start_node->var_type = start_lit->var_type;
                            start_node->val.int_val = start_val;
                            decl->initializer = (ASTNode*)start_node;

                            WhileNode *wh = parser_alloc(p, sizeof(WhileNode));
                            wh->base.type = NODE_WHILE;

                            BinaryOpNode *cond = parser_alloc(p, sizeof(BinaryOpNode));
                            cond->base.type = NODE_BINARY_OP;
                            cond->op = (step > 0) ? TOKEN_LTE : TOKEN_GTE;
                            VarRefNode *ref_i = parser_alloc(p, sizeof(VarRefNode));
                            ref_i->base.type = NODE_VAR_REF;
                            ref_i->name = parser_strdup(p, "i");
                            cond->left = (ASTNode*)ref_i;
                            LiteralNode *end_node = parser_alloc(p, sizeof(LiteralNode));
                            end_node->base.type = NODE_LITERAL;
                            end_node->var_type = start_lit->var_type;
                            end_node->val.int_val = end_val;
                            cond->right = (ASTNode*)end_node;
                            wh->condition = (ASTNode*)cond;

                            EmitNode *em = parser_alloc(p, sizeof(EmitNode));
                            em->base.type = NODE_EMIT;
                            VarRefNode *ref_i2 = parser_alloc(p, sizeof(VarRefNode));
                            ref_i2->base.type = NODE_VAR_REF;
                            ref_i2->name = parser_strdup(p, "i");
                            em->value = (ASTNode*)ref_i2;

                            AssignNode *inc = parser_alloc(p, sizeof(AssignNode));
                            inc->base.type = NODE_ASSIGN;
                            inc->op = TOKEN_PLUS_ASSIGN;
                            VarRefNode *ref_i3 = parser_alloc(p, sizeof(VarRefNode));
                            ref_i3->base.type = NODE_VAR_REF;
                            ref_i3->name = parser_strdup(p, "i");
                            inc->target = (ASTNode*)ref_i3;
                            LiteralNode *step_node = parser_alloc(p, sizeof(LiteralNode));
                            step_node->base.type = NODE_LITERAL;
                            step_node->var_type = start_lit->var_type;
                            step_node->val.int_val = step;
                            inc->value = (ASTNode*)step_node;

                            em->base.next = (ASTNode*)inc;
                            wh->body = (ASTNode*)em;

                            decl->base.next = (ASTNode*)wh;
                            fn->body = (ASTNode*)decl;

                            fn->base.next = p->synthetic_classes;
                            p->synthetic_classes = (ASTNode*)fn;

                            CallNode *call = parser_alloc(p, sizeof(CallNode));
                            call->base.type = NODE_CALL;
                            call->name = parser_strdup(p, flux_name);
                            call->target = NULL;
                            call->args = NULL;

                            *curr_elem = (ASTNode*)call;
                            curr_elem = &(*curr_elem)->next;
                        } else {
                            start_lit->val.int_val = start_val;
                            int i = start_val + step;
                            while (step > 0 ? (i <= end_val) : (i >= end_val)) {
                                LiteralNode *new_lit = parser_alloc(p, sizeof(LiteralNode));
                                new_lit->base.type = NODE_LITERAL;
                                new_lit->var_type = start_lit->var_type;
                                new_lit->val.int_val = i;
                                (*curr_elem)->next = (ASTNode*)new_lit;
                                curr_elem = &(*curr_elem)->next;
                                i += step;
                            }
                            curr_elem = &(*curr_elem)->next;
                        }
                    } else {
                        *curr_elem = NULL;
                    }
                } else {
                    parser_fail(p, "Range bounds must be integers");
                }
            } else {
                parser_fail(p, "Range bounds in array literals must be compile-time literals for now");
            }
        } else {
            curr_elem = &(*curr_elem)->next;
        }
      }
    }

    if (restore_space_call) {
        p->disable_space_call--;
    }

    eat(p, TOKEN_RBRACKET);
    ArrayLitNode *an = parser_alloc(p, sizeof(ArrayLitNode));
    an->base.type = NODE_ARRAY_LIT;
    an->elements = elems_head;
    an->is_vector = p->settings.allow_vector_initialization;
    node = (ASTNode*)an;
    set_loc(node, line, col);
  }
  else if (p->current_token.type == TOKEN_NUMBER ||
           p->current_token.type == TOKEN_UINT_LIT ||
           p->current_token.type == TOKEN_LONG_LIT ||
           p->current_token.type == TOKEN_ULONG_LIT ||
           p->current_token.type == TOKEN_LONG_LONG_LIT ||
           p->current_token.type == TOKEN_ULONG_LONG_LIT) {
    LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
    ln->base.type = NODE_LITERAL;

    if (p->current_token.type == TOKEN_UINT_LIT) { ln->var_type.base = TYPE_INT; ln->var_type.is_unsigned = 1; }
    else if (p->current_token.type == TOKEN_LONG_LIT) { ln->var_type.base = TYPE_LONG; }
    else if (p->current_token.type == TOKEN_ULONG_LIT) { ln->var_type.base = TYPE_LONG; ln->var_type.is_unsigned = 1; }
    else if (p->current_token.type == TOKEN_LONG_LONG_LIT) { ln->var_type.base = TYPE_LONG_LONG; }
    else if (p->current_token.type == TOKEN_ULONG_LONG_LIT) { ln->var_type.base = TYPE_LONG_LONG; ln->var_type.is_unsigned = 1; }
    else { ln->var_type.base = TYPE_INT; }

    ln->val.long_val = p->current_token.long_val;
    eat(p, p->current_token.type);
    node = (ASTNode*)ln;
    set_loc(node, line, col);
  }

  else if (p->current_token.type == TOKEN_SINGLE_LIT || p->current_token.type == TOKEN_LONG_DOUBLE_LIT || p->current_token.type == TOKEN_DOUBLE_LIT) {
    LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
    ln->base.type = NODE_LITERAL;
    if (p->current_token.type == TOKEN_LONG_DOUBLE_LIT) {
        ln->var_type.base = TYPE_LONG_DOUBLE;
        ln->val.double_val = p->current_token.double_val;
    } else if (p->current_token.type == TOKEN_DOUBLE_LIT) {
        ln->var_type.base = TYPE_DOUBLE;
        ln->val.double_val = p->current_token.double_val;
    } else {
        ln->var_type.base = TYPE_SINGLE;
        ln->val.single_val = (float)p->current_token.double_val;
    }
    eat(p, p->current_token.type);
    node = (ASTNode*)ln;
    set_loc(node, line, col);
  }
  else if (p->current_token.type == TOKEN_CHAR_LIT) {
    LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
    ln->base.type = NODE_LITERAL;
    ln->var_type.base = TYPE_CHAR;
    ln->val.long_val = p->current_token.int_val;
    eat(p, TOKEN_CHAR_LIT);
    node = (ASTNode*)ln;
    set_loc(node, line, col);
  }
  else if (p->current_token.type == TOKEN_STRING) {
    if (p->l->settings.double_quote_as_string) {
      // Treat as string(c"...")

      // 1. Create argument C-string literal node: c"..."
      LiteralNode *arg_ln = parser_alloc(p, sizeof(LiteralNode));
      arg_ln->base.type = NODE_LITERAL;
      arg_ln->var_type.base = TYPE_CHAR;
      arg_ln->var_type.ptr_depth = 1;
      arg_ln->val.str_val = parser_strdup(p, p->current_token.text);
      arg_ln->base.next = NULL;
      set_loc((ASTNode*)arg_ln, line, col);

      // 2. Create target class/function variable reference: string
      VarRefNode *target_vn = parser_alloc(p, sizeof(VarRefNode));
      target_vn->base.type = NODE_VAR_REF;
      target_vn->name = parser_strdup(p, "string");
      set_loc((ASTNode*)target_vn, line, col);

      // 3. Create CallNode: string(c"...")
      CallNode *call_node = parser_alloc(p, sizeof(MethodCallNode));
      call_node->base.type = NODE_CALL;
      call_node->name = parser_strdup(p, "string");
      call_node->target = (ASTNode*)target_vn;
      call_node->args = (ASTNode*)arg_ln;
      set_loc((ASTNode*)call_node, line, col);

      p->current_token.text = NULL;
      eat(p, TOKEN_STRING);
      node = (ASTNode*)call_node;
    } else {
      // Treat as C-string: c"..."
      LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
      ln->base.type = NODE_LITERAL;
      ln->var_type.base = TYPE_CHAR;
      ln->var_type.ptr_depth = 1;
      ln->val.str_val = parser_strdup(p, p->current_token.text);
      p->current_token.text = NULL;
      eat(p, TOKEN_STRING);
      node = (ASTNode*)ln;
      set_loc(node, line, col);
    }
  }
  else if (p->current_token.type == TOKEN_C_STRING) {
    LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
    ln->base.type = NODE_LITERAL;
    ln->var_type.base = TYPE_CHAR;
    ln->var_type.ptr_depth = 1;
    ln->val.str_val = parser_strdup(p, p->current_token.text);
    p->current_token.text = NULL;
    eat(p, TOKEN_C_STRING);
    node = (ASTNode*)ln;
    set_loc(node, line, col);
  }
  else if (p->current_token.type == TOKEN_BYTE_STRING) {
    LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
    ln->base.type = NODE_LITERAL;
    ln->var_type.base = TYPE_CLASS;
    ln->var_type.class_name = parser_strdup(p, "byte_string");
    ln->val.str_val = parser_strdup(p, p->current_token.text);
    p->current_token.text = NULL;
    eat(p, TOKEN_BYTE_STRING);
    node = (ASTNode*)ln;
    set_loc(node, line, col);
  }
  else if (p->current_token.type == TOKEN_TRUE || p->current_token.type == TOKEN_FALSE) {
    LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
    ln->base.type = NODE_LITERAL;
    ln->var_type.base = TYPE_BOOL;
    ln->val.long_val = (p->current_token.type == TOKEN_TRUE) ? 1 : 0;
    eat(p, p->current_token.type);
    node = (ASTNode*)ln;
    set_loc(node, line, col);
  }
  else if (p->current_token.type == TOKEN_KW_SIZEOF || p->current_token.type == TOKEN_KW_ALIGNOF) {
    int is_align = (p->current_token.type == TOKEN_KW_ALIGNOF);
    eat(p, p->current_token.type);

    int has_paren = (p->current_token.type == TOKEN_LPAREN);
    if (has_paren) eat(p, TOKEN_LPAREN);

    SizeOfNode *sn = parser_alloc(p, sizeof(SizeOfNode));
    sn->base.type = is_align ? NODE_ALIGNOF : NODE_SIZEOF;
    sn->target_type.base = TYPE_UNKNOWN;

    if (is_type_start(p)) {
        sn->target_type = parse_type(p);
    } else {
        sn->operand = parse_expression(p);
    }

    if (has_paren) eat(p, TOKEN_RPAREN);

    node = (ASTNode*)sn;
    set_loc(node, line, col);
  }
  else if (p->current_token.type == TOKEN_KW_ISCOMPATIBLE) {
    eat(p, TOKEN_KW_ISCOMPATIBLE);
    eat(p, TOKEN_LPAREN);
    IsCompatibleNode *icn = parser_alloc(p, sizeof(IsCompatibleNode));
    icn->base.type = NODE_ISCOMPATIBLE;
    icn->target_type = parse_type(p);
    eat(p, TOKEN_COMMA);
    icn->target_type2 = parse_type(p);
    eat(p, TOKEN_RPAREN);
    node = (ASTNode*)icn;
    set_loc(node, line, col);
  }
  else if (p->current_token.type == TOKEN_IDENTIFIER || p->current_token.type == TOKEN_ELLIPSIS) {
    char *name;
    if (p->current_token.type == TOKEN_ELLIPSIS) {
        name = parser_strdup(p, "...");
        eat(p, TOKEN_ELLIPSIS);
    } else {
        name = parser_strdup(p, p->current_token.text);
        p->current_token.text = NULL;
        eat(p, TOKEN_IDENTIFIER);
    }

    VarRefNode *vn = parser_alloc(p, sizeof(VarRefNode));
    vn->base.type = NODE_VAR_REF;
    vn->name = name;
    node = (ASTNode*)vn;
    set_loc(node, line, col);

  }
  else if (is_type_start(p)) {
      VarType t = parse_type(p);
      LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
      ln->base.type = NODE_LITERAL;
      ln->var_type.base = TYPE_INT;
      ln->var_type.ptr_depth = 0;
      ln->val.long_val = t.base;
      node = (ASTNode*)ln;
      set_loc(node, line, col);
  }
  else {
    char msg[128];
    const char *tok = p->current_token.text ? p->current_token.text : token_type_to_string(p->current_token.type);
    snprintf(msg, sizeof(msg), "Unexpected token in expression: '%s'", tok);
    parser_fail(p, msg);
    return NULL;
  }

  if (p->settings.multiplication_if_digit_word &&
      node && node->type == NODE_LITERAL) {
      VarType lt = ((LiteralNode*)node)->var_type;
      int is_numeric = (lt.base == TYPE_INT || lt.base == TYPE_LONG ||
                        lt.base == TYPE_LONG_LONG || lt.base == TYPE_UNSIGNED_INT ||
                        lt.base == TYPE_UNSIGNED_LONG || lt.base == TYPE_UNSIGNED_LONG_LONG ||
                        lt.base == TYPE_SINGLE || lt.base == TYPE_DOUBLE || lt.base == TYPE_LONG_DOUBLE);
      if (is_numeric &&
          p->current_token.type == TOKEN_IDENTIFIER &&
          !p->current_token.has_space_before) {
          char *name = parser_strdup(p, p->current_token.text);
          eat(p, TOKEN_IDENTIFIER);
          VarRefNode *vr = parser_alloc(p, sizeof(VarRefNode));
          vr->base.type = NODE_VAR_REF;
          vr->name = name;
          BinaryOpNode *bn = parser_alloc(p, sizeof(BinaryOpNode));
          bn->base.type = NODE_BINARY_OP;
          bn->op = TOKEN_STAR;
          bn->left = node;
          bn->right = (ASTNode*)vr;
          set_loc((ASTNode*)bn, line, col);
          node = (ASTNode*)bn;
      }
  }

  if (p->settings.exponentation_if_word_digit &&
      node && node->type == NODE_VAR_REF &&
      !p->current_token.has_space_before) {
      TokenType t = p->current_token.type;
      int is_num = (t == TOKEN_NUMBER || t == TOKEN_UINT_LIT || t == TOKEN_LONG_LIT ||
                    t == TOKEN_ULONG_LIT || t == TOKEN_LONG_LONG_LIT || t == TOKEN_ULONG_LONG_LIT ||
                    t == TOKEN_SINGLE_LIT || t == TOKEN_DOUBLE_LIT || t == TOKEN_LONG_DOUBLE_LIT);
      if (is_num) {
          // TODO: implement ** operator for exponentiation
      }
  }

  return parse_postfix(p, node);
}

ASTNode* parse_unary(Parser *p) {
  if (p->has_error) return NULL;
  int line = p->current_token.line;
  int col = p->current_token.col;

  if (p->current_token.type == TOKEN_INCREMENT || p->current_token.type == TOKEN_DECREMENT) {
      int op = p->current_token.type;
      eat(p, op);
      ASTNode *operand = parse_unary(p);
      IncDecNode *node = parser_alloc(p, sizeof(IncDecNode));
      node->base.type = NODE_INC_DEC;
      node->target = operand;
      node->is_prefix = 1;
      node->op = op;
      set_loc((ASTNode*)node, line, col);
      return (ASTNode*)node;
  }

  if (p->current_token.type == TOKEN_NOT || p->current_token.type == TOKEN_MINUS ||
      p->current_token.type == TOKEN_BIT_NOT || p->current_token.type == TOKEN_STAR ||
      p->current_token.type == TOKEN_AND) {
    int op = p->current_token.type;
    eat(p, op);
    ASTNode *operand = parse_unary(p);
    UnaryOpNode *node = parser_alloc(p, sizeof(UnaryOpNode));
    node->base.type = NODE_UNARY_OP;
    node->op = op;
    node->operand = operand;
    set_loc((ASTNode*)node, line, col);
    return (ASTNode*)node;
  }

  if (p->current_token.type == TOKEN_LPAREN) {
      eat(p, TOKEN_LPAREN);
      ASTNode *expr = parse_expression(p);
      eat(p, TOKEN_RPAREN);
      if (!expr) { p->has_error = 1; return NULL; }
      return parse_postfix(p, expr);
  }

  return parse_factor(p);
}

static ASTNode* parse_binary_op(Parser *p, ASTNode* (*sub_parser)(Parser*), TokenType* ops, int num_ops) {
  ASTNode *left = sub_parser(p);
  while (1) { if (p->has_error) break;
    int found = 0;
    int line = p->current_token.line;
    int col = p->current_token.col;
    for (int i = 0; i < num_ops; i++) {
      if (p->current_token.type == ops[i]) {
        found = 1;
        TokenType op = p->current_token.type;
        eat(p, op);
        ASTNode *right = sub_parser(p);
        BinaryOpNode *node = parser_alloc(p, sizeof(BinaryOpNode));
        node->base.type = NODE_BINARY_OP;
        node->op = op;
        node->left = left;
        node->right = right;
        set_loc((ASTNode*)node, line, col);
        left = (ASTNode*)node;
        break;
      }
    }
    if (!found) break;
  }
  return left;
}

ASTNode* parse_term(Parser *p) {
  TokenType ops[] = {TOKEN_STAR, TOKEN_SLASH, TOKEN_MOD};
  return parse_binary_op(p, parse_unary, ops, 3);
}
ASTNode* parse_additive(Parser *p) {
  TokenType ops[] = {TOKEN_PLUS, TOKEN_MINUS};
  return parse_binary_op(p, parse_term, ops, 2);
}
ASTNode* parse_shift(Parser *p) {
  TokenType ops[] = {TOKEN_LSHIFT, TOKEN_RSHIFT, TOKEN_LROTATE, TOKEN_RROTATE};
  return parse_binary_op(p, parse_additive, ops, 4);
}
ASTNode* parse_relational(Parser *p) {
  TokenType ops[] = {TOKEN_LT, TOKEN_GT, TOKEN_LTE, TOKEN_GTE};
  return parse_binary_op(p, parse_shift, ops, 4);
}
ASTNode* parse_equality(Parser *p) {
  TokenType ops[] = {TOKEN_EQ, TOKEN_NEQ};
  return parse_binary_op(p, parse_relational, ops, 2);
}
ASTNode* parse_bitwise_and(Parser *p) {
  TokenType ops[] = {TOKEN_AND};
  return parse_binary_op(p, parse_equality, ops, 1);
}
ASTNode* parse_bitwise_xor(Parser *p) {
  TokenType ops[] = {TOKEN_XOR};
  return parse_binary_op(p, parse_bitwise_and, ops, 1);
}
ASTNode* parse_bitwise_or(Parser *p) {
  TokenType ops[] = {TOKEN_OR};
  return parse_binary_op(p, parse_bitwise_xor, ops, 1);
}
ASTNode* parse_logic_and(Parser *p) {
  TokenType ops[] = {TOKEN_AND_AND};
  return parse_binary_op(p, parse_bitwise_or, ops, 1);
}
ASTNode* parse_logic_or(Parser *p) {
  TokenType ops[] = {TOKEN_OR_OR};
  return parse_binary_op(p, parse_logic_and, ops, 1);
}

ASTNode* parse_fallback(Parser *p) {
  ASTNode *left = parse_logic_or(p);
  while (p->current_token.type == TOKEN_QUESTION || p->current_token.type == TOKEN_QUESTION_QUESTION) { if (p->has_error) break;
      int is_coalesce = (p->current_token.type == TOKEN_QUESTION_QUESTION);
      int op = p->current_token.type;
      int line = p->current_token.line;
      int col = p->current_token.col;
      eat(p, op);

      char *err_id = NULL;
      char **err_names = NULL;
      int num_err = 0;
      char *err_var = NULL;
      int is_default = 0;

      if (is_coalesce) {
          err_id = parser_strdup(p, "ErrNull");
          err_names = parser_alloc_raw(p, sizeof(char*));
          err_names[0] = parser_strdup(p, "ErrNull");
          num_err = 1;
      } else if (p->current_token.type == TOKEN_LBRACKET) {
          eat(p, TOKEN_LBRACKET);
          int cap = 4;
          err_names = parser_alloc_raw(p, sizeof(char*) * cap);
          while (p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF) { if (p->has_error) break;
              if (p->current_token.type != TOKEN_IDENTIFIER) {
                  parser_fail(p, "Expected error name in ? [...] case");
                  break;
              }
              if (num_err >= cap) {
                  cap *= 2;
                  char **tmp = parser_alloc_raw(p, sizeof(char*) * cap);
                  for (int i = 0; i < num_err; i++) tmp[i] = err_names[i];
                  err_names = tmp;
              }
              err_names[num_err++] = parser_strdup(p, p->current_token.text);
              eat(p, TOKEN_IDENTIFIER);
              if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA);
              else break;
          }
          eat(p, TOKEN_RBRACKET);
          err_id = num_err > 0 ? parser_strdup(p, err_names[0]) : NULL;
          // optional bound error variable: ? [ErrX] v
          if (p->current_token.type == TOKEN_IDENTIFIER) {
              err_var = parser_strdup(p, p->current_token.text);
              eat(p, TOKEN_IDENTIFIER);
          }
      } else {
          // `? ...` with no bracket => default catch-all case.
          is_default = 1;
      }

      ASTNode *right = parse_logic_or(p);

      BinaryOpNode *node = parser_alloc(p, sizeof(BinaryOpNode));
      node->base.type = NODE_BINARY_OP;
      node->base.line = line;
      node->base.col = col;
      node->op = op;
      node->left = left;
      node->right = right;
      node->fallback_err_name = err_id;
      node->err_var_name = err_var;

      ResidueCase *rc = parser_alloc_raw(p, sizeof(ResidueCase));
      rc->err_names = err_names;
      rc->num_err = num_err;
      rc->body = right;
      rc->next = NULL;
      rc->is_default = is_default;
      node->cases = rc;

      left = (ASTNode*)node;
  }
  return left;
}

ASTNode* parse_dollar(Parser *p) {
  ASTNode *lhs = parse_fallback(p);
  if (p->has_error) return lhs;

  if (p->current_token.type == TOKEN_DOLLAR) {
      int line = p->current_token.line;
      int col = p->current_token.col;
      eat(p, TOKEN_DOLLAR);

      ASTNode *rhs = parse_dollar(p); // Right-associative

      if (lhs && lhs->type == NODE_MEMBER_ACCESS) {
          MemberAccessNode *ma = (MemberAccessNode*)lhs;
          MethodCallNode *mc = parser_alloc(p, sizeof(MethodCallNode));
          mc->base.type = NODE_METHOD_CALL;
          mc->object = ma->object;
          mc->method_name = ma->member_name;
          mc->args = rhs;
          mc->mangled_name = NULL;
          mc->owner_class = NULL;
          mc->is_static = 0;
          set_loc((ASTNode*)mc, line, col);
          return (ASTNode*)mc;
      }

      CallNode *node = parser_alloc(p, sizeof(MethodCallNode));
      node->base.type = NODE_CALL;
      node->name = NULL;
      node->target = lhs;

      if (lhs && lhs->type == NODE_VAR_REF) {
          node->name = ((VarRefNode*)lhs)->name;
          node->target = NULL;
      }

      node->args = rhs;
      set_loc((ASTNode*)node, line, col);
      return (ASTNode*)node;
  }
  return lhs;
}

ASTNode* parse_assignment(Parser *p) {
  ASTNode *lhs = parse_dollar(p);
  if (p->has_error) return lhs;

  if (p->current_token.type == TOKEN_ASSIGN ||
      p->current_token.type == TOKEN_PLUS_ASSIGN ||
      p->current_token.type == TOKEN_MINUS_ASSIGN ||
      p->current_token.type == TOKEN_STAR_ASSIGN ||
      p->current_token.type == TOKEN_SLASH_ASSIGN ||
      p->current_token.type == TOKEN_MOD_ASSIGN ||
      p->current_token.type == TOKEN_AND_ASSIGN ||
      p->current_token.type == TOKEN_OR_ASSIGN ||
      p->current_token.type == TOKEN_XOR_ASSIGN ||
      p->current_token.type == TOKEN_LSHIFT_ASSIGN ||
      p->current_token.type == TOKEN_RSHIFT_ASSIGN) {

      int line = p->current_token.line;
      int col = p->current_token.col;
      int op = p->current_token.type;
      eat(p, op);

      ASTNode *rhs = parse_assignment(p);

      AssignNode *node = parser_alloc(p, sizeof(AssignNode));
      node->base.type = NODE_ASSIGN;
      node->value = rhs;
      node->op = op;

      if (lhs->type == NODE_VAR_REF) {
          node->name = ((VarRefNode*)lhs)->name;
          ((VarRefNode*)lhs)->name = NULL;
          // No free
      } else {
          node->target = lhs;
      }
      set_loc((ASTNode*)node, line, col);
      return (ASTNode*)node;
  }
  return lhs;
}

ASTNode* parse_expression(Parser *p) {
  if (p->has_error) return NULL;
  return parse_assignment(p);
}
ASTNode* parse_initializer(Parser *p, VarType vtype) {
    if (p->current_token.type == TOKEN_ASSIGN) {
        eat(p, TOKEN_ASSIGN);
        return parse_expression(p);
    } else if (p->current_token.type == TOKEN_LPAREN) {
        eat(p, TOKEN_LPAREN);
        ASTNode *args_head = NULL;
        ASTNode **curr_arg = &args_head;
        if (p->current_token.type != TOKEN_RPAREN) {
            while (1) { if (p->has_error) break;
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
        CallNode *cnode = parser_alloc(p, sizeof(MethodCallNode));
        cnode->base.type = NODE_CALL;
        cnode->base.line = p->current_token.line;
        cnode->base.col = p->current_token.col;
        if (vtype.class_name) {
            char cls_name[1024];
            snprintf(cls_name, sizeof(cls_name), "%s", vtype.class_name);
            char *bracket = strchr(cls_name, '[');
            if (bracket) {
                *bracket = '\0';
                TemplateInstNode *ti = parser_alloc(p, sizeof(TemplateInstNode));
                ti->base.type = NODE_TEMPLATE_INSTANTIATION;
                VarRefNode *vr = parser_alloc(p, sizeof(VarRefNode));
                vr->base.type = NODE_VAR_REF;
                vr->name = parser_strdup(p, cls_name);
                ti->target = (ASTNode*)vr;
                // Parse the types from the string! No, that's too hard.
                // Instead, just pass the mangled name directly if we can't parse it?
                // Actually, since this is a known limitation, let's just use the string and let semantic fix it.
            }
        }

        cnode->name = vtype.class_name ? parser_strdup(p, vtype.class_name) : NULL;
        cnode->target = NULL;

        // Actually, if it has brackets, Semantic can just try replacing '[' with '_' and ']' with ''.

        cnode->args = args_head;
        return (ASTNode*)cnode;
    }
    return NULL;
}
