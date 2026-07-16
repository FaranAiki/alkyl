#include "parser_internal.h"
#include <string.h>
#include <stdlib.h>


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
    while (p->current_token.type == TOKEN_COMMA) {
      eat(p, TOKEN_COMMA);
      expr = parse_expression(p);
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
    }
  }
  eat(p, TOKEN_RPAREN);
  CallNode *node = parser_alloc(p, sizeof(CallNode));
  node->base.type = NODE_CALL;
  node->name = name;
  node->target = target;
  node->args = args_head;
  return (ASTNode*)node;
}

ASTNode* parse_postfix(Parser *p, ASTNode *node) {
    while (1) {
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
                    while (p->current_token.type == TOKEN_COMMA) {
                        eat(p, TOKEN_COMMA);
                        expr = parse_expression(p);
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
        else if (p->current_token.type == TOKEN_LBRACKET) {
            eat(p, TOKEN_LBRACKET);
            
            if (is_type_start(p)) {
                int max_args = 16;
                VarType *types = parser_alloc(p, sizeof(VarType) * max_args);
                int num_types = 0;
                
                while (p->current_token.type != TOKEN_RBRACKET) {
                    types[num_types++] = parse_type(p);
                    if (p->current_token.type == TOKEN_COMMA) {
                        eat(p, TOKEN_COMMA);
                    } else {
                        break;
                    }
                }
                eat(p, TOKEN_RBRACKET);
                
                TemplateInstNode *ti = parser_alloc(p, sizeof(TemplateInstNode));
                ti->base.type = NODE_TEMPLATE_INSTANTIATION;
                ti->target = node;
                ti->template_types = types;
                ti->num_template_types = num_types;
                node = (ASTNode*)ti;
            } else {
                ASTNode *index = parse_expression(p);
                eat(p, TOKEN_RBRACKET);
                
                ArrayAccessNode *aa = parser_alloc(p, sizeof(ArrayAccessNode));
                aa->base.type = NODE_ARRAY_ACCESS;
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
        else if (p->current_token.type == TOKEN_LPAREN) {
            node = parse_call(p, node);
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

  if (p->current_token.type == TOKEN_TYPEOF) {
      eat(p, TOKEN_TYPEOF);
      int has_paren = (p->current_token.type == TOKEN_LPAREN);
      if (has_paren) eat(p, TOKEN_LPAREN);
      if (is_type_start(p)) {
          VarType t = parse_type(p);
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
  else if (p->current_token.type == TOKEN_LBRACKET) {
    eat(p, TOKEN_LBRACKET);
    ASTNode *elems_head = NULL;
    ASTNode **curr_elem = &elems_head;
    if (p->current_token.type != TOKEN_RBRACKET) {
      *curr_elem = parse_expression(p);
      curr_elem = &(*curr_elem)->next;
      while (p->current_token.type == TOKEN_COMMA) {
        eat(p, TOKEN_COMMA);
        *curr_elem = parse_expression(p);
        curr_elem = &(*curr_elem)->next;
      }
    }
    eat(p, TOKEN_RBRACKET);
    ArrayLitNode *an = parser_alloc(p, sizeof(ArrayLitNode));
    an->base.type = NODE_ARRAY_LIT;
    an->elements = elems_head;
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
  else if (p->current_token.type == TOKEN_NULL) {
    LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
    ln->base.type = NODE_LITERAL;
    ln->var_type.base = TYPE_VOID;
    ln->var_type.ptr_depth = 1;
    ln->val.long_val = 0;
    eat(p, TOKEN_NULL);
    node = (ASTNode*)ln;
    set_loc(node, line, col);
  }
  else if (p->current_token.type == TOKEN_FLOAT || p->current_token.type == TOKEN_LONG_DOUBLE_LIT) {
    LiteralNode *ln = parser_alloc(p, sizeof(LiteralNode));
    ln->base.type = NODE_LITERAL;
    if (p->current_token.type == TOKEN_LONG_DOUBLE_LIT) ln->var_type.base = TYPE_LONG_DOUBLE;
    else ln->var_type.base = TYPE_DOUBLE;
    ln->val.double_val = p->current_token.double_val;
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
      CallNode *call_node = parser_alloc(p, sizeof(CallNode));
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
  else if (p->current_token.type == TOKEN_IDENTIFIER) {
    char *name = parser_strdup(p, p->current_token.text);
    p->current_token.text = NULL;
    eat(p, TOKEN_IDENTIFIER);
    
    VarRefNode *vn = parser_alloc(p, sizeof(VarRefNode));
    vn->base.type = NODE_VAR_REF;
    vn->name = name;
    node = (ASTNode*)vn;
    set_loc(node, line, col);
    
  }
  else {
    char msg[128];
    const char *tok = p->current_token.text ? p->current_token.text : token_type_to_string(p->current_token.type);
    snprintf(msg, sizeof(msg), "Unexpected token in expression: '%s'", tok);
    parser_fail(p, msg);
    return NULL; 
  }
  
  return parse_postfix(p, node);
}

ASTNode* parse_unary(Parser *p) {
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
      return parse_postfix(p, expr);
  }

  return parse_factor(p);
}

static ASTNode* parse_binary_op(Parser *p, ASTNode* (*sub_parser)(Parser*), TokenType* ops, int num_ops) {
  ASTNode *left = sub_parser(p);
  while (1) {
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
  TokenType ops[] = {TOKEN_QUESTION};
  return parse_binary_op(p, parse_logic_or, ops, 1);
}

ASTNode* parse_assignment(Parser *p) {
  ASTNode *lhs = parse_fallback(p); 
  
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
        cnode->base.line = p->current_token.line;
        cnode->base.col = p->current_token.col;
        if (vtype.class_name) {
            char *cls_name = parser_strdup(p, vtype.class_name);
            char *bracket = strchr(cls_name, '[');
            if (bracket) {
                // If it's something like Vector[int], construct a dummy TemplateInstNode so Semantic Analyzer can process it
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
