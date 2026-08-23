/**
 * @file class.c
 * @brief Class, struct, and enum parsing implementation for the Alkyl parser.
 */
#include "parser_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

ASTNode* parse_class_impl(Parser *p, int modifiers);

/**
 * @brief Parses an enum definition.
 * @param p Parser context.
 * @return AST node for the enum, or NULL on error.
 */
ASTNode* parse_enum(Parser *p) {
  eat(p, TOKEN_ENUM);
  char *enum_name = NULL;
  if (p->current_token.type == TOKEN_IDENTIFIER) {
      enum_name = parser_strdup(p, p->current_token.text);
      eat(p, TOKEN_IDENTIFIER);
      register_typename(p, enum_name, 1);
  } else if (p->current_token.type == TOKEN_LBRACKET) {
      static int anon_enum_counter = 0;
      char buf[64];
      snprintf(buf, sizeof(buf), "__AnonEnum_%d", anon_enum_counter++);
      enum_name = parser_strdup(p, buf);
      register_typename(p, enum_name, 1);
  } else {
      parser_fail(p, "Expected enum name or '[' for anonymous enum");
  }

  eat(p, TOKEN_LBRACKET);

  EnumEntry *entries_head = NULL;
  EnumEntry **curr_entry = &entries_head;
  int current_val = 0;

  while (p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF) { if (p->has_error) break;
      if (p->current_token.type != TOKEN_IDENTIFIER && p->current_token.text == NULL) parser_fail(p, "Expected enum member name");
      char *member_name = parser_strdup(p, p->current_token.text);
      eat(p, p->current_token.type);

      if (p->current_token.type == TOKEN_ASSIGN) {
          eat(p, TOKEN_ASSIGN);
          int sign = 1;
          if (p->current_token.type == TOKEN_MINUS) { sign = -1; eat(p, TOKEN_MINUS); }
          if (p->current_token.type != TOKEN_NUMBER) parser_fail(p, "Expected integer value for enum member");
          current_val = p->current_token.int_val * sign;
          eat(p, TOKEN_NUMBER);
      }

      EnumEntry *entry = parser_alloc_raw(p, sizeof(EnumEntry));
      entry->name = member_name;
      entry->value = current_val;
      entry->next = NULL;
      *curr_entry = entry;
      curr_entry = &entry->next;

      current_val++;

      if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA);
      else if (p->current_token.type != TOKEN_RBRACKET) parser_fail(p, "Expected ',' or ']' in enum definition");
  }
  eat(p, TOKEN_RBRACKET);
  eat_semi(p);

  EnumNode *en = parser_alloc(p, sizeof(EnumNode));
  en->base.type = NODE_ENUM;
  en->name = enum_name;
  en->entries = entries_head;
  return (ASTNode*)en;
}

/**
 * @brief Core implementation for parsing a class, struct, or union.
 * @param p Parser context.
 * @param modifiers Pre-parsed modifier flags.
 * @return AST node for the class, or NULL on error.
 */
ASTNode* parse_class_impl(Parser *p, int modifiers) {
  int is_open = 0;
  if (modifiers & MODIFIER_OPEN) is_open = 1;
  else if (modifiers & MODIFIER_CLOSED) is_open = 0;

  if (p->current_token.type == TOKEN_OPEN) { is_open = 1; eat(p, TOKEN_OPEN); }
  else if (p->current_token.type == TOKEN_CLOSED) { is_open = 0; eat(p, TOKEN_CLOSED); }

  // TODO reformat this
  if (p->current_token.type == TOKEN_CLASS || p->current_token.type == TOKEN_STRUCT || p->current_token.type == TOKEN_UNION) {
      int is_union = (p->current_token.type == TOKEN_UNION);
      eat(p, p->current_token.type);

      int is_tainted_class = 0;
      if (p->current_token.type == TOKEN_QUESTION) {
          is_tainted_class = 1;
          eat(p, TOKEN_QUESTION);
      }

      if (p->current_token.type != TOKEN_IDENTIFIER && 
          p->current_token.type != TOKEN_KW_INT && 
          p->current_token.type != TOKEN_KW_CHAR && 
          p->current_token.type != TOKEN_KW_BOOL && 
          p->current_token.type != TOKEN_KW_SINGLE && 
          p->current_token.type != TOKEN_KW_DOUBLE && 
          p->current_token.type != TOKEN_KW_SHORT && 
          p->current_token.type != TOKEN_KW_LONG && 
          p->current_token.type != TOKEN_KW_VOID) {
          parser_fail(p, "Expected name after 'class', 'struct' or 'union'");
      }
      char *class_name = parser_strdup(p, p->current_token.text ? p->current_token.text : token_type_to_string(p->current_token.type));
      eat(p, p->current_token.type);

      if (p->current_token.type == TOKEN_QUESTION) {
          is_tainted_class = 1;
          eat(p, TOKEN_QUESTION);
      }

      if (p->current_token.type == TOKEN_LBRACKET || p->current_token.type == TOKEN_LT) {
          char full_class_name[1024];
          snprintf(full_class_name, sizeof(full_class_name), "%s", class_name);
          TokenType end_token = (p->current_token.type == TOKEN_LBRACKET) ? TOKEN_RBRACKET : TOKEN_GT;
          char start_char = (p->current_token.type == TOKEN_LBRACKET) ? '[' : '<';
          char end_char = (p->current_token.type == TOKEN_LBRACKET) ? ']' : '>';
          eat(p, p->current_token.type);
          size_t fn_len = strlen(full_class_name);
          if (fn_len + 1 < sizeof(full_class_name)) {
              full_class_name[fn_len] = start_char; full_class_name[fn_len + 1] = '\0';
          }
          while (p->current_token.type != end_token && p->current_token.type != TOKEN_EOF) {
              fn_len = strlen(full_class_name);
              if (fn_len + 1 < sizeof(full_class_name)) {
                  const char *txt = (p->current_token.text) ? p->current_token.text : ((p->current_token.type == TOKEN_NUMBER) ? "0" : token_type_to_string(p->current_token.type));
                  snprintf(full_class_name + fn_len, sizeof(full_class_name) - fn_len, "%s", txt);
              }
              eat(p, p->current_token.type);
          }
          eat(p, end_token);
          fn_len = strlen(full_class_name);
          if (fn_len + 1 < sizeof(full_class_name)) {
              full_class_name[fn_len] = end_char; full_class_name[fn_len + 1] = '\0';
          }
          class_name = parser_strdup(p, full_class_name);
      }

      register_typename(p, class_name, 0);

      char *parent_name = NULL;
      if (p->current_token.type == TOKEN_IS) {
          eat(p, TOKEN_IS);
          if (p->current_token.type != TOKEN_IDENTIFIER) parser_fail(p, "Expected parent class name after 'is'");
          parent_name = parser_strdup(p, p->current_token.text);
          eat(p, TOKEN_IDENTIFIER);
      }

      char **traits = NULL;
      int trait_count = 0;
      if (p->current_token.type == TOKEN_HAS) {
          eat(p, TOKEN_HAS);
          int cap = 4;
          traits = parser_alloc_raw(p, sizeof(char*) * cap);
          do {
              if (p->current_token.type != TOKEN_IDENTIFIER) parser_fail(p, "Expected trait or struct name after 'has'");
              if (trait_count >= cap) {
                  cap *= 2;
                  char **new_traits = parser_alloc_raw(p, sizeof(char*)*cap);
                  memcpy(new_traits, traits, sizeof(char*)*trait_count);
                  traits = new_traits;
              }
              traits[trait_count++] = parser_strdup(p, p->current_token.text);
              eat(p, TOKEN_IDENTIFIER);
              if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA);
              else break;
          } while(1);
      }

      eat(p, TOKEN_LBRACE);

      ASTNode *members_head = NULL;
      ASTNode **curr_member = &members_head;

      int label_modifier_stack[16];
      int stack_depth = 0;
      label_modifier_stack[0] = 0;

      while ((p->current_token.type != TOKEN_RBRACE || stack_depth > 0) && p->current_token.type != TOKEN_EOF) { 
          if (p->has_error) break;

          if (p->current_token.type == TOKEN_RBRACE && stack_depth > 0) {
              eat(p, TOKEN_RBRACE);
              stack_depth--;
              continue;
          }

          int member_modifiers = parse_modifiers(p);

          if (p->current_token.type == TOKEN_COLON) {
              eat(p, TOKEN_COLON);
              label_modifier_stack[stack_depth] = member_modifiers;
              continue;
          }

          if (p->current_token.type == TOKEN_LBRACE && member_modifiers != 0) {
              eat(p, TOKEN_LBRACE);
              stack_depth++;
              label_modifier_stack[stack_depth] = label_modifier_stack[stack_depth-1] | member_modifiers;
              continue;
          }

          member_modifiers |= label_modifier_stack[stack_depth];

          int member_open = is_open;
          if (member_modifiers & MODIFIER_OPEN) member_open = 1;
          else if (member_modifiers & MODIFIER_CLOSED) member_open = 0;

          if (p->current_token.type == TOKEN_OPEN) { member_open = 1; eat(p, TOKEN_OPEN); }
          else if (p->current_token.type == TOKEN_CLOSED) { member_open = 0; eat(p, TOKEN_CLOSED); }

          int line = p->current_token.line;
          int col = p->current_token.col;

          int is_compound = 0;
          char **type_params = NULL;
          VarType **allowed_types = NULL;
          int *num_allowed = NULL;
          int num_type_params = 0;

          if (p->current_token.type == TOKEN_COMPOUND) {
              is_compound = 1;
              eat(p, TOKEN_COMPOUND);
              eat(p, TOKEN_LBRACKET);
              int max_params = 16;
              type_params = parser_alloc_raw(p, sizeof(char*) * max_params);
              allowed_types = parser_alloc_raw(p, sizeof(VarType*) * max_params);
              num_allowed = parser_alloc_raw(p, sizeof(int) * max_params);

              while (p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF) {
                  if (p->has_error) break;
                  VarType *curr_allowed = NULL;
                  int curr_num = 0;
                  if (p->current_token.type == TOKEN_IDENTIFIER && p->current_token.text && streq_lit(p->current_token.text, "type")) {
                      eat(p, TOKEN_IDENTIFIER);
                      if (p->current_token.type == TOKEN_LBRACKET) {
                          eat(p, TOKEN_LBRACKET);
                          curr_allowed = parser_alloc_raw(p, sizeof(VarType) * 16);
                          while (p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF) {
                              curr_allowed[curr_num++] = parse_type(p);
                              if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA);
                              else break;
                          }
                          eat(p, TOKEN_RBRACKET);
                      }
                  } else {
                      parser_fail(p, "Expected 'type' keyword in compound");
                  }
                  if (p->current_token.type != TOKEN_IDENTIFIER) parser_fail(p, "Expected type parameter name in compound");
                  char *type_param = parser_strdup(p, p->current_token.text);
                  type_params[num_type_params] = type_param;
                  allowed_types[num_type_params] = curr_allowed;
                  num_allowed[num_type_params] = curr_num;
                  num_type_params++;
                  eat(p, TOKEN_IDENTIFIER);
                  
                  register_typename(p, type_param, 0);
                  
                  if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA);
                  else break;
              }
              eat(p, TOKEN_RBRACKET);
          }

#define APPEND_MEMBER(node_ptr) do { \
    ASTNode *_node = (ASTNode*)(node_ptr); \
    if (is_compound) { \
        CompoundNode *cn = parser_alloc(p, sizeof(CompoundNode)); \
        cn->base.type = NODE_COMPOUND; \
        cn->base.line = line; \
        cn->base.col = col; \
        cn->type_params = type_params; \
        cn->allowed_types = allowed_types; \
        cn->num_allowed = num_allowed; \
        cn->num_type_params = num_type_params; \
        cn->body = _node; \
        _node = (ASTNode*)cn; \
        is_compound = 0; \
    } \
    *curr_member = _node; \
    curr_member = &_node->next; \
} while(0)

          if (p->current_token.type == TOKEN_FLUX) {
              eat(p, TOKEN_FLUX);
              VarType vt = parse_type(p);

              eat(p, TOKEN_LBRACE);
              ASTNode *body = parse_statements(p);
              eat(p, TOKEN_RBRACE);
              apply_implicit_return(p, &body);

              FuncDefNode *func = parser_alloc(p, sizeof(FuncDefNode));
              func->base.type = NODE_FUNC_DEF;
              func->base.line = line;
              func->base.col = col;
              func->name = parser_strdup(p, "iterate");
              func->ret_type = vt;
              func->params = NULL;
              func->body = body;
              func->has_body = 1;
              func->is_open = member_open;
              func->class_name = parser_strdup(p, class_name);
              func->is_flux = 1;

              apply_func_modifiers(func, member_modifiers);

              APPEND_MEMBER(func);
              continue;
          }

          if (p->current_token.type == TOKEN_AS) {
              eat(p, TOKEN_AS);
              VarType target_type = parse_type(p);

              // No parameters
              eat(p, TOKEN_LBRACE);
              ASTNode *body = parse_statements(p);
              eat(p, TOKEN_RBRACE);
              apply_implicit_return(p, &body);

              FuncDefNode *func = parser_alloc(p, sizeof(FuncDefNode));
              func->base.type = NODE_FUNC_DEF;
              func->base.line = line;
              func->base.col = col;

              // Create name: "as_<type_str>"
              // To do this simply, we can use the class_name if TYPE_CLASS, or a basic map
              char as_name[256];
              if (target_type.base == TYPE_CLASS || target_type.base == TYPE_UNKNOWN) {
                  snprintf(as_name, sizeof(as_name), "as_%s", target_type.class_name);
              } else if (target_type.base == TYPE_INT) {
                  snprintf(as_name, sizeof(as_name), "as_int");
              } else if (target_type.base == TYPE_SINGLE) {
                  snprintf(as_name, sizeof(as_name), "as_float");
              } else {
                  snprintf(as_name, sizeof(as_name), "as_type%d", target_type.base);
              }

              func->name = parser_strdup(p, as_name);
              func->ret_type = target_type;
              func->params = NULL;
              func->body = body;
              func->has_body = 1;
              func->is_open = member_open;
              func->class_name = parser_strdup(p, class_name);

              apply_func_modifiers(func, member_modifiers);

              APPEND_MEMBER(func);
              continue;
          }

          VarType vt = parse_type(p);
          member_modifiers |= parse_modifiers(p);
          debug_parser("after parse_type, vt.base=%d, token.type=%d\n", vt.base, p->current_token.type);
          if (vt.base != TYPE_UNKNOWN || (vt.base == TYPE_UNKNOWN && vt.class_name != NULL)) {
              if (p->current_token.type == TOKEN_LPAREN) {
                  Token next = parser_peek_token(p);
                  if (next.type == TOKEN_STAR) {
                      char *mem_name = NULL;
                      vt = parse_func_ptr_decl(p, vt, &mem_name);

                      ASTNode *init = parse_initializer(p, vt);
                      eat_semi(p);

                      VarDeclNode *var = parser_alloc(p, sizeof(VarDeclNode));
                      var->base.type = NODE_VAR_DECL;
                      var->base.line = line;
                      var->base.col = col;
                      var->name = mem_name;
                      var->var_type = vt;
                      var->initializer = init;
                      var->is_mutable = 1;
                      var->is_open = member_open;

                      apply_var_modifiers(var, member_modifiers);

                      APPEND_MEMBER(var);
                      continue;
                  } else {
                      if (vt.class_name != NULL) {
                  debug_parser("constructor check: vt.class_name='%s', class_name='%s'\n", vt.class_name, class_name);
                      }
                      if ((vt.base == TYPE_CLASS || vt.base == TYPE_UNKNOWN) && vt.class_name != NULL &&
                             (streq_lit(vt.class_name, class_name) ||
                              (strlen(vt.class_name) > strlen(class_name) && streq_lit(vt.class_name + strlen(vt.class_name) - strlen(class_name), class_name)))) {
                      // Constructor detected: ClassName(...)
                      char *mem_name = parser_strdup(p, vt.class_name);
                      vt.base = TYPE_VOID; // Constructors implicitly return void or handle specially

                      eat(p, TOKEN_LPAREN);
                      Parameter *params = NULL;
                      Parameter **curr_p = &params;

                      if (p->current_token.type != TOKEN_RPAREN) {
                          while(1) {
                              VarType pt = parse_type(p);
                              if(pt.base == TYPE_UNKNOWN) parser_fail(p, "Expected parameter type in method declaration");
                              char *pname = parser_strdup(p, p->current_token.text);
                              eat(p, TOKEN_IDENTIFIER);

                              if (p->current_token.type == TOKEN_LBRACKET) {
                                  eat(p, TOKEN_LBRACKET);
                                  if (p->current_token.type != TOKEN_RBRACKET) {
                                      ASTNode *sz = parse_expression(p);
                                      (void)sz;
                                  }
                                  eat(p, TOKEN_RBRACKET);
                                  pt.ptr_depth++;
                              }

                              Parameter *pm = parser_alloc_raw(p, sizeof(Parameter));
                              pm->type = pt; pm->name = pname;
                              if (p->current_token.type == TOKEN_ASSIGN) {
                                  eat(p, TOKEN_ASSIGN);
                                  pm->default_value = parse_expression(p);
                              } else {
                                  pm->default_value = NULL;
                              }
                              *curr_p = pm; curr_p = &pm->next;
                              if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA); else break;
                          }
                      }
                      eat(p, TOKEN_RPAREN);
                      eat(p, TOKEN_LBRACE);
                      ASTNode *body = parse_statements(p);
                      eat(p, TOKEN_RBRACE);
                      apply_implicit_return(p, &body);

                      FuncDefNode *func = parser_alloc(p, sizeof(FuncDefNode));
                      func->base.type = NODE_FUNC_DEF;
                      func->base.line = line;
                      func->base.col = col;
                      func->name = mem_name;
                      func->ret_type = vt;
                      func->params = params;
                      func->body = body;
                      func->has_body = 1;
                      func->is_open = member_open;
                      func->class_name = parser_strdup(p, class_name);

                      apply_func_modifiers(func, member_modifiers);

                      APPEND_MEMBER(func);
                      continue;
                  }
                  }
              }

              char *mem_name = NULL;
              if (p->current_token.type == TOKEN_PREFOP || p->current_token.type == TOKEN_INFOP || p->current_token.type == TOKEN_SUFFOP ||
                  p->current_token.type == TOKEN_PREMUT || p->current_token.type == TOKEN_INFMUT || p->current_token.type == TOKEN_SUFMUT) {
                  int kind = p->current_token.type;
                  eat(p, kind);
                  eat(p, TOKEN_LBRACKET);
                  TokenType op_type = p->current_token.type;
                  char name_buf[64];
                  snprintf(name_buf, sizeof(name_buf), "__op_%d_%d", kind, op_type);
                  mem_name = parser_strdup(p, name_buf);
                  eat(p, op_type);
                  eat(p, TOKEN_RBRACKET);
              } else {
                  if (p->current_token.type != TOKEN_IDENTIFIER) {
                      parser_fail(p, "Expected member name in class body");
                      mem_name = parser_strdup(p, "__error_name");
                  } else {
                      mem_name = parser_strdup(p, p->current_token.text);
                      eat(p, TOKEN_IDENTIFIER);
                  }
              }
              if (p->current_token.type == TOKEN_LPAREN) {
                  eat(p, TOKEN_LPAREN);
                  Parameter *params = NULL;
                  Parameter **curr_p = &params;

                  if (p->current_token.type != TOKEN_RPAREN) {
                      while(1) {
                          int pmods = parse_modifiers(p);
                          VarType pt = parse_type(p);
                          if(pt.base == TYPE_UNKNOWN) parser_fail(p, "Expected parameter type in method declaration");
                          char *pname = parser_strdup(p, p->current_token.text);
                          eat(p, TOKEN_IDENTIFIER);

                          if (p->current_token.type == TOKEN_LBRACKET) {
                              eat(p, TOKEN_LBRACKET);
                              if (p->current_token.type != TOKEN_RBRACKET) {
                                  ASTNode *sz = parse_expression(p);
                                  (void)sz;
                              }
                              eat(p, TOKEN_RBRACKET);
                              pt.ptr_depth++;
                          }

                          Parameter *pm = parser_alloc_raw(p, sizeof(Parameter));
                          pm->type = pt; pm->name = pname;
                          apply_param_modifiers(pm, pmods);
                          if (p->current_token.type == TOKEN_ASSIGN) {
                              eat(p, TOKEN_ASSIGN);
                              pm->default_value = parse_expression(p);
                          } else {
                              pm->default_value = NULL;
                          }
                          *curr_p = pm; curr_p = &pm->next;
                          if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA); else break;
                      }
                  }
                  eat(p, TOKEN_RPAREN);
                  ASTNode *body = NULL;
                  int has_body = 0;
                  if (p->current_token.type == TOKEN_SEMICOLON) {
                      eat_semi(p);
                      has_body = 0;
                  } else {
                      eat(p, TOKEN_LBRACE);
                      has_body = 1;
                      body = parse_statements(p);
                      eat(p, TOKEN_RBRACE);
                      apply_implicit_return(p, &body);
                  }

                  FuncDefNode *func = parser_alloc(p, sizeof(FuncDefNode));
                  func->base.type = NODE_FUNC_DEF;
                  func->base.line = line;
                  func->base.col = col;
                  func->name = mem_name;
                  func->ret_type = vt;
                  func->params = params;
                  func->body = body;
                  func->has_body = has_body;
                  func->is_open = member_open;
                  func->class_name = parser_strdup(p, class_name);

                  apply_func_modifiers(func, member_modifiers);

                  APPEND_MEMBER(func);
              } else {
                  int next_extra_ptrs = 0;
                  while(1) {
                      VarType current_vtype = vt;
                      current_vtype.ptr_depth += next_extra_ptrs;

                      int is_array = 0;
                      ASTNode *array_size = NULL;
                      ASTNode **curr_sz = &array_size;

                      while (p->current_token.type == TOKEN_LBRACKET) { if (p->has_error) break;
                          is_array = 1;
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

                      VarDeclNode *var = parser_alloc(p, sizeof(VarDeclNode));
                      var->base.type = NODE_VAR_DECL;
                      var->base.line = line;
                      var->base.col = col;
                      var->name = mem_name;
                      var->var_type = current_vtype;
                      var->initializer = init;
                      var->is_mutable = 1;
                      var->is_open = member_open;
                      var->is_array = is_array;
                      var->array_size = array_size;

                      apply_var_modifiers(var, member_modifiers);

                      APPEND_MEMBER(var);

                      if (p->current_token.type == TOKEN_COMMA) {
                          eat(p, TOKEN_COMMA);

                          next_extra_ptrs = 0;
                          while (p->current_token.type == TOKEN_STAR) { if (p->has_error) break;
                              next_extra_ptrs++;
                              eat(p, TOKEN_STAR);
                          }

                          if (p->current_token.type != TOKEN_IDENTIFIER) parser_fail(p, "Expected identifier after comma");
                          mem_name = parser_strdup(p, p->current_token.text);
                          p->current_token.text = NULL;
                          eat(p, TOKEN_IDENTIFIER);
                      } else {
                          break;
                      }
                  }
                  eat_semi(p);
              }
          } else {
              parser_fail(p, "Unexpected token in class body. Expected member declaration or '}'.");
          }
      }
      eat(p, TOKEN_RBRACE);

      ClassNode *cls = parser_alloc(p, sizeof(ClassNode));
      cls->base.type = NODE_CLASS;
      cls->has_body = 1;
      cls->name = class_name;
      cls->parent_name = parent_name;
      cls->traits.names = traits;
      cls->traits.count = trait_count;
      cls->members = members_head;
      cls->is_open = is_open;
      cls->is_union = is_union;
      cls->is_tainted = is_tainted_class;

      apply_class_modifiers(cls, modifiers);
      return (ASTNode*)cls;
  }
  return NULL;
}

/**
 * @brief Parses a class, struct, or union declaration (parses modifiers first).
 * @param p Parser context.
 * @return AST node for the class, or NULL on error.
 */
ASTNode* parse_class(Parser *p) {
    return parse_class_impl(p, parse_modifiers(p));
}
