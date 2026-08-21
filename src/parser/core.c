/**
 * @file core.c
 * @brief Core parser implementation for the Alkyl compiler.
 */
#include "parser_internal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

/**
 * @brief Initializes a Parser with a lexer and settings.
 * @param p Parser to initialize.
 * @param l Lexer to use.
 * @param settings Parser settings (may be NULL for defaults).
 */
void parser_init(Parser *p, Lexer *l, ParserSettings *settings) {
    p->l = l;
    p->ctx = l->ctx;
    p->has_error = 0;
    p->macro_head = NULL;
    p->type_head = NULL;
    p->alias_head = NULL;
    p->expansion_head = NULL;
    p->disable_macro_expansion = 0;

    p->tokens = NULL;
    p->token_count = 0;
    p->token_capacity = 0;
    p->token_pos = 0;
    p->synthetic_classes = NULL;
    p->in_space_separated_call = 0;
    p->disable_space_call = 0;
    p->pending_cconv = NULL;

    if (p->ctx && p->ctx->arena) {
        hashmap_init(&p->types_map, p->ctx->arena, 64);
    } else {
        hashmap_init(&p->types_map, NULL, 64);
    }

    if (settings) {
        p->settings = *settings;
    } else {
        // Defaults
        p->settings.require_parens_for_conditions = 0;
        p->settings.allow_implicit_return = 0;
        p->settings.allow_postfix_types = 0;
        p->settings.strict_boolean_conditions = 0;
        p->settings.namespace_auto_search = 1;
        p->settings.namespace_ausearch_warning = 1;
        p->settings.function_call_require_comma = 1;
        p->settings.array_separator_with_space = 0;
        p->settings.multiplication_if_digit_word = 0;
        p->settings.exponentation_if_word_digit = 0;
        p->settings.function_auto_call = 0;
    }

    if (l) {
        p->current_token.type = TOKEN_UNKNOWN;
    }
}

/**
 * @brief Allocates memory for a parser AST node, zeroing it and attaching source info.
 * @param p Parser context.
 * @param size Number of bytes to allocate.
 * @return Pointer to zeroed memory.
 */
void* parser_alloc(Parser *p, size_t size) {
    if (!p || !p->ctx || !p->ctx->arena) return calloc(1, size);
    void *ptr = arena_alloc(p->ctx->arena, size);
    if (ptr) {
        memset(ptr, 0, size);
        if (p->l) {
            ((ASTNode*)ptr)->filename = (char*)p->l->filename;
            ((ASTNode*)ptr)->source = (char*)p->l->src;
        }
    }
    return ptr;
}

/**
 * @brief Allocates raw memory for parser use, zeroing it.
 * @param p Parser context.
 * @param size Number of bytes to allocate.
 * @return Pointer to zeroed memory.
 */
void* parser_alloc_raw(Parser *p, size_t size) {
    if (!p || !p->ctx || !p->ctx->arena) return calloc(1, size);
    void *ptr = arena_alloc(p->ctx->arena, size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

/**
 * @brief Duplicates a string using the parser's arena allocator.
 * @param p Parser context.
 * @param str String to duplicate.
 * @return Duplicated string, or NULL if str is NULL.
 */
char* parser_strdup(Parser *p, const char *str) {
    if (!str) return NULL;
    if (!p || !p->ctx || !p->ctx->arena) return strdup(str);
    return arena_strdup(p->ctx->arena, str);
}

/**
 * @brief Registers a type name in the parser's type map.
 * @param p Parser context.
 * @param name Name of the type.
 * @param is_enum Non-zero if the type is an enum.
 */
void register_typename(Parser *p, const char *name, int is_enum) {
    hashmap_put(&p->types_map, name, (void*)(intptr_t)(is_enum ? 2 : 1));

    const char *current_ns = diag_get_namespace(p->ctx);
    if (current_ns && strlen(current_ns) > 0) {
        char full_name[512];
        snprintf(full_name, sizeof(full_name), "%s.%s", current_ns, name);
        hashmap_put(&p->types_map, full_name, (void*)(intptr_t)(is_enum ? 2 : 1));
    }
}

/**
 * @brief Checks whether a name is registered as a type.
 * @param p Parser context.
 * @param name Type name to check.
 * @return Non-zero if the name is a known type.
 */
int is_typename(Parser *p, const char *name) {
    return hashmap_has(&p->types_map, name);
}

/**
 * @brief Checks whether the current token starts a type declaration.
 * @param p Parser context.
 * @return Non-zero if the current token begins a type.
 */
int is_type_start(Parser *p) {
    TokenType ct = p->current_token.type;
    if (ct == TOKEN_KW_INT || ct == TOKEN_KW_SHORT || ct == TOKEN_KW_LONG ||
        ct == TOKEN_KW_DOUBLE || ct == TOKEN_KW_SINGLE || ct == TOKEN_KW_CHAR ||
        ct == TOKEN_KW_VOID || ct == TOKEN_KW_BOOL || ct == TOKEN_KW_UNSIGNED || ct == TOKEN_KW_SIGNED) {
        return 1;
    }
    if (ct == TOKEN_IDENTIFIER) {
        if (is_typename(p, p->current_token.text)) {
            return 1;
        }
        
        // Peek ahead for namespaced types e.g. std.string
        Lexer l_copy = *p->l;
        Token next1 = lexer_next(&l_copy);
        if (next1.type == TOKEN_DOT) {
            Token next2 = lexer_next(&l_copy);
            if (next2.type == TOKEN_IDENTIFIER) {
                char full_name[512];
                snprintf(full_name, sizeof(full_name), "%s.%s", p->current_token.text, next2.text);
                if (is_typename(p, full_name)) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int get_typename_kind(Parser *p, const char *name) {
    if (hashmap_has(&p->types_map, name)) {
        return (int)(intptr_t)hashmap_get(&p->types_map, name);
    }
    return 0;
}

/**
 * @brief Registers or updates a type alias.
 * @param p Parser context.
 * @param name Alias name.
 * @param target Target VarType for the alias.
 */
void register_alias(Parser *p, const char *name, VarType target) {
    TypeAlias *curr = p->alias_head;
    while(curr) {
        if (streq_lit(curr->name, name)) {
            curr->target = target;
            return;
        }
        curr = curr->next;
    }

    TypeAlias *a = parser_alloc_raw(p, sizeof(TypeAlias));
    a->name = parser_strdup(p, name);
    a->target = target;
    if (target.class_name) a->target.class_name = parser_strdup(p, target.class_name);

    a->next = p->alias_head;
    p->alias_head = a;
}

/**
 * @brief Looks up a registered type alias.
 * @param p Parser context.
 * @param name Alias name to look up.
 * @return Pointer to the alias VarType, or NULL if not found.
 */
VarType* get_alias(Parser *p, const char *name) {
    TypeAlias *curr = p->alias_head;
    while(curr) {
        if (streq_lit(curr->name, name)) return &curr->target;
        curr = curr->next;
    }
    return NULL;
}

/**
 * @brief Clones a token, duplicating its text via the parser allocator.
 * @param p Parser context.
 * @param t Token to clone.
 * @return Cloned token.
 */
Token token_clone(Parser *p, Token t) {
    Token new_t = t;
    if (t.text) new_t.text = parser_strdup(p, t.text);
    return new_t;
}

/**
 * @brief Registers a macro in the parser's macro table.
 * @param p Parser context.
 * @param name Macro name.
 * @param params Parameter names (NULL for object-like macros).
 * @param param_count Number of parameters.
 * @param body Replacement tokens for the macro body.
 * @param body_len Length of the body token array.
 */
void register_macro(Parser *p, const char *name, char **params, int param_count, Token *body, int body_len) {
    Macro *m = parser_alloc_raw(p, sizeof(Macro));
    m->name = parser_strdup(p, name);
    m->params = params;
    m->param_count = param_count;
    m->body = parser_alloc_raw(p, sizeof(Token) * body_len);
    for (int i=0; i<body_len; i++) {
        m->body[i] = token_clone(p, body[i]);
    }
    m->body_len = body_len;
    m->next = p->macro_head;
    p->macro_head = m;
    if (p->ctx) p->ctx->macro_head = m;
}

static Macro* find_macro(Parser *p, const char *name) {
    Macro *curr = p->macro_head;
    while(curr) {
        if (streq_lit(curr->name, name)) return curr;
        curr = curr->next;
    }
    return NULL;
}

/* True if `name` is already being expanded (C preprocessor blue-paint). */
static int macro_is_expanding(Parser *p, const char *name) {
    for (Expansion *e = p->expansion_head; e; e = e->next) {
        if (e->macro_name && streq_lit(e->macro_name, name)) return 1;
    }
    return 0;
}

/**
 * @brief Returns the next raw token from the parser's token buffer.
 * @param p Parser context.
 * @return Next token, or TOKEN_EOF if exhausted.
 */
Token lexer_next_raw(Parser *p) {
    if (p->tokens && p->token_pos < p->token_count) {
        return p->tokens[p->token_pos++];
    }
    Token eof;
    memset(&eof, 0, sizeof(Token));
    eof.type = TOKEN_EOF;
    return eof;
}

/**
 * @brief Returns the next token, expanding macros if applicable.
 * @param p Parser context.
 * @return Next expanded token.
 */
Token get_next_token_expanded(Parser *p) {
    if (p->expansion_head) {
        if (p->expansion_head->pos < p->expansion_head->count) {
            return token_clone(p, p->expansion_head->tokens[p->expansion_head->pos++]);
        } else {
            p->expansion_head = p->expansion_head->next;
            return get_next_token_expanded(p);
        }
    }
    return lexer_next_raw(p);
}

static Token fetch_safe(Parser *p) { return get_next_token_expanded(p); }

/* Expand object/function-like macros starting at token `t`. Used by eat() and
 * parse_program so the first token of a REPL line also expands (e.g. define aliases). */
/**
 * @brief Expands macros starting from a given token.
 * @param p Parser context.
 * @param t Starting token for expansion.
 * @return The first token after expansion.
 */
Token expand_macros_from(Parser *p, Token t) {
    while (!p->disable_macro_expansion && t.type == TOKEN_IDENTIFIER) {
        Macro *m = find_macro(p, t.text);
        if (!m || macro_is_expanding(p, m->name)) break;

        Token **args = NULL;
        int *arg_lens = NULL;

        if (m->param_count > 0) {
            Token peek = fetch_safe(p);
            if (peek.type != TOKEN_LPAREN) {
                parser_fail(p, "Function-like macro requires arguments list '('.");
                return t;
            }

            args = parser_alloc_raw(p, sizeof(Token*) * m->param_count);
            arg_lens = parser_alloc_raw(p, m->param_count * sizeof(int));

            for (int i = 0; i < m->param_count; i++) {
                int cap = 8; int len = 0;
                args[i] = parser_alloc_raw(p, sizeof(Token) * cap);
                int depth = 0;
                while (1) {
                    Token arg_t = fetch_safe(p);
                    if (arg_t.type == TOKEN_EOF) {
                        parser_fail(p, "Unexpected EOF in macro arguments");
                        return t;
                    }

                    if (arg_t.type == TOKEN_LPAREN) depth++;
                    else if (arg_t.type == TOKEN_RPAREN) {
                        if (depth == 0) {
                            if (i == m->param_count - 1) break;
                            depth--;
                        } else depth--;
                    }
                    else if (arg_t.type == TOKEN_COMMA) {
                        if (depth == 0) {
                            if (i < m->param_count - 1) break;
                        }
                    }

                    if (len >= cap) {
                        cap *= 2;
                        Token *new_arr = parser_alloc_raw(p, sizeof(Token) * cap);
                        memcpy(new_arr, args[i], sizeof(Token) * len);
                        args[i] = new_arr;
                    }
                    args[i][len++] = arg_t;
                }
                arg_lens[i] = len;
            }
        }

        int res_cap = m->body_len * 2 + 16;
        int res_len = 0;
        Token *res = parser_alloc_raw(p, sizeof(Token) * res_cap);

        for (int i = 0; i < m->body_len; i++) {
            Token bt = m->body[i];
            int p_idx = -1;
            if (bt.type == TOKEN_IDENTIFIER && m->param_count > 0) {
                for (int k = 0; k < m->param_count; k++) {
                    if (streq_lit(bt.text, m->params[k])) { p_idx = k; break; }
                }
            }

            if (p_idx != -1) {
                for (int k = 0; k < arg_lens[p_idx]; k++) {
                    if (res_len >= res_cap) {
                        res_cap *= 2;
                        Token *new_res = parser_alloc_raw(p, sizeof(Token) * res_cap);
                        memcpy(new_res, res, sizeof(Token) * res_len);
                        res = new_res;
                    }
                    res[res_len++] = token_clone(p, args[p_idx][k]);
                }
            } else {
                if (res_len >= res_cap) {
                    res_cap *= 2;
                    Token *new_res = parser_alloc_raw(p, sizeof(Token) * res_cap);
                    memcpy(new_res, res, sizeof(Token) * res_len);
                    res = new_res;
                }
                res[res_len++] = token_clone(p, bt);
            }
        }

        Expansion *ex = parser_alloc_raw(p, sizeof(Expansion));
        ex->tokens = res;
        ex->count = res_len;
        ex->pos = 0;
        ex->macro_name = m->name;
        ex->next = p->expansion_head;
        p->expansion_head = ex;

        t = fetch_safe(p);
    }
    return t;
}

/**
 * @brief Reports a parse error at a specific token and increments the error count.
 * @param p Parser context.
 * @param t Token where the error occurred.
 * @param msg Error message.
 */
void parser_fail_at(Parser *p, Token t, const char *msg) {
    report_error(p->l, t, msg);
    if (p->ctx) p->ctx->error_count++;
    p->has_error = 1;
}

/**
 * @brief Reports a parse error at the current token.
 * @param p Parser context.
 * @param msg Error message.
 */
void parser_fail(Parser *p, const char *msg) {
    parser_fail_at(p, p->current_token, msg);
}

/**
 * @brief Synchronizes the parser after an error by consuming tokens until a safe resync point.
 * @param p Parser context.
 */
void parser_sync(Parser *p) {
    p->has_error = 0; // Clear error so eat() can consume tokens
    while (p->current_token.type != TOKEN_EOF) {
        if (p->current_token.type == TOKEN_SEMICOLON) {
            eat_semi(p);
            return;
        }
        if (p->current_token.type == TOKEN_RBRACE) {
            eat(p, TOKEN_RBRACE);
            return;
        }
        switch (p->current_token.type) {
            case TOKEN_CLASS:
            case TOKEN_STRUCT:
            case TOKEN_UNION:
            case TOKEN_NAMESPACE:
            case TOKEN_KW_INT:
            case TOKEN_KW_VOID:
            case TOKEN_KW_CHAR:
            case TOKEN_KW_BOOL:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_LOOP:
            case TOKEN_RETURN:
            case TOKEN_KW_LET:
            case TOKEN_DEFINE:
                return;
            default:
                eat(p, p->current_token.type);
                p->has_error = 0;
                break;
        }
    }
}

/**
 * @brief Consumes the current token if it matches the expected type, otherwise reports an error.
 * @param p Parser context.
 * @param type Expected token type.
 */
void eat(Parser *p, TokenType type) {
  if (p->has_error) return;
  if (p->current_token.type == type) {
    p->current_token = expand_macros_from(p, fetch_safe(p));
  } else {
    char msg[256];
    const char *expected = get_token_description(type);
    const char *found = p->current_token.type == TOKEN_EOF ? "end of file" :
                        (p->current_token.text ? p->current_token.text : token_type_to_string(p->current_token.type));

    snprintf(msg, sizeof(msg), "Expected '%s' but found '%s'", expected, found);
    parser_fail(p, msg);
  }
}

// Composite type parsing helper
VarType parse_func_ptr_decl(Parser *p, VarType ret_type, char **out_name);
VarType parse_func_sig_decl(Parser *p, VarType ret_type, char **out_name);
static Token parser_peek_past_parens(Parser *p);

/**
 * @brief Parses a type specification from the current token stream.
 * @param p Parser context.
 * @return Parsed VarType.
 */
VarType parse_type(Parser *p) {
  VarType t = {0};
  t.base = TYPE_UNKNOWN;

  if (p->current_token.type == TOKEN_KW_UNSIGNED) {
      t.is_unsigned = 1;
      eat(p, TOKEN_KW_UNSIGNED);
    if (p->has_error) return (VarType){0};
  } else if (p->current_token.type == TOKEN_KW_SIGNED) {
      eat(p, TOKEN_KW_SIGNED);
    if (p->has_error) return (VarType){0};
  }

  if (p->current_token.type == TOKEN_ENUM) {
      eat(p, TOKEN_ENUM);
      char *enum_name;
      if (p->current_token.type == TOKEN_IDENTIFIER) {
          enum_name = parser_strdup(p, p->current_token.text);
          eat(p, TOKEN_IDENTIFIER);
      } else if (p->current_token.type == TOKEN_LBRACKET) {
          static int anon_enum_counter = 0;
          char buf[64];
          snprintf(buf, sizeof(buf), "__AnonEnum_%d", anon_enum_counter++);
          enum_name = parser_strdup(p, buf);
      } else {
          parser_fail(p, "Expected enum name or '[' for anonymous enum");
          return t;
      }
      register_typename(p, enum_name, 1);
      eat(p, TOKEN_LBRACKET);
      while (p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF) { if (p->has_error) break;
          if (p->current_token.type != TOKEN_IDENTIFIER) { parser_fail(p, "Expected enum member name"); break; }
          eat(p, TOKEN_IDENTIFIER);
          if (p->current_token.type == TOKEN_ASSIGN) {
              eat(p, TOKEN_ASSIGN);
              if (p->current_token.type == TOKEN_MINUS) eat(p, TOKEN_MINUS);
              if (p->current_token.type == TOKEN_NUMBER) eat(p, TOKEN_NUMBER);
          }
          if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA);
          else if (p->current_token.type != TOKEN_RBRACKET) { parser_fail(p, "Expected ',' or ']' in enum"); break; }
      }
      eat(p, TOKEN_RBRACKET);
      t.base = TYPE_ENUM;
      t.class_name = enum_name;
      return t;
  }

  if (p->current_token.type == TOKEN_IDENTIFIER) {
      VarType *alias = get_alias(p, p->current_token.text);
      if (alias) {
          t.base = alias->base;
          t.ptr_depth += alias->ptr_depth;
          t.array_size = alias->array_size;
          if (alias->class_name) t.class_name = parser_strdup(p, alias->class_name);
          eat(p, TOKEN_IDENTIFIER);
    if (p->has_error) return (VarType){0};
      }
      else {
          int saved_pos = p->token_pos;
          Token saved_tok = p->current_token;
          Expansion *saved_exp = p->expansion_head;
          int saved_exp_pos = saved_exp ? saved_exp->pos : -1;

           char full_type_name[512];
           snprintf(full_type_name, sizeof(full_type_name), "%s", p->current_token.text);
           eat(p, TOKEN_IDENTIFIER);
           if (p->has_error) return (VarType){0};

           while (p->current_token.type == TOKEN_DOT) { if (p->has_error) break;
               eat(p, TOKEN_DOT);
               if (p->has_error) return (VarType){0};
               size_t len = strlen(full_type_name);
               if (len + 1 < sizeof(full_type_name)) {
                   snprintf(full_type_name + len, sizeof(full_type_name) - len, ".%s", p->current_token.text);
               }
               if (p->current_token.type == TOKEN_IDENTIFIER) {
                   eat(p, TOKEN_IDENTIFIER);
                   if (p->has_error) return (VarType){0};
               } else {
                   break;
               }
           }

           int kind = get_typename_kind(p, full_type_name);
           if (kind != 0) {
               if (kind == 2) {
                   t.base = TYPE_ENUM;
                   t.class_name = parser_strdup(p, full_type_name);
               } else {
                   t.base = TYPE_CLASS;
                   char base_name[512];
                   snprintf(base_name, sizeof(base_name), "%s", full_type_name);

                   if (p->current_token.type == TOKEN_LBRACKET || p->current_token.type == TOKEN_LT) {
                       char full_name[1024];
                       snprintf(full_name, sizeof(full_name), "%s", base_name);

                       TokenType end_token = (p->current_token.type == TOKEN_LBRACKET) ? TOKEN_RBRACKET : TOKEN_GT;
                       char start_char = (p->current_token.type == TOKEN_LBRACKET) ? '[' : '<';
                       char end_char = (p->current_token.type == TOKEN_LBRACKET) ? ']' : '>';

                       eat(p, p->current_token.type);
                       if (p->has_error) return (VarType){0};
                       size_t fn_len = strlen(full_name);
                       if (fn_len + 1 < sizeof(full_name)) {
                           full_name[fn_len] = start_char;
                           full_name[fn_len + 1] = '\0';
                       }

                        while (p->current_token.type != end_token && p->current_token.type != TOKEN_EOF) {
                            fn_len = strlen(full_name);
                            if (fn_len + 1 < sizeof(full_name)) {
                                const char *txt;
                                if (p->current_token.text) {
                                    txt = p->current_token.text;
                                } else if (p->current_token.type == TOKEN_NUMBER) {
                                    static char num_buf[32];
                                    snprintf(num_buf, sizeof(num_buf), "%lld", p->current_token.long_val);
                                    txt = num_buf;
                                } else {
                                    txt = token_type_to_string(p->current_token.type);
                                }
                                snprintf(full_name + fn_len, sizeof(full_name) - fn_len, "%s", txt);
                            }
                            eat(p, p->current_token.type);
                            if (p->has_error) return (VarType){0};
                        }
                       eat(p, end_token);
                       if (p->has_error) return (VarType){0};
                       fn_len = strlen(full_name);
                       if (fn_len + 1 < sizeof(full_name)) {
                           full_name[fn_len] = end_char;
                           full_name[fn_len + 1] = '\0';
                       }
                       t.class_name = parser_strdup(p, full_name);
                   } else {
                       t.class_name = parser_strdup(p, base_name);
                   }
              }
          } else {
              /* Rewind speculative type parse. Must restore macro expansion
               * state too — otherwise define aliases like
               * `define fp as __c_lib.printf` lose the `.printf` tokens. */
              p->token_pos = saved_pos;
              p->current_token = saved_tok;
              p->expansion_head = saved_exp;
              if (saved_exp && saved_exp_pos >= 0) saved_exp->pos = saved_exp_pos;
              if (t.is_unsigned) t.base = TYPE_INT;
              return t;
          }
      }
  } else {
      TokenType ct = p->current_token.type;
      if (ct == TOKEN_KW_INT) { t.base = TYPE_INT; eat(p, TOKEN_KW_INT); }
      else if (ct == TOKEN_KW_SHORT) {
          t.base = TYPE_SHORT;
          eat(p, TOKEN_KW_SHORT);
          if (p->current_token.type == TOKEN_KW_INT) {
              eat(p, TOKEN_KW_INT);
          }
      }
      else if (ct == TOKEN_KW_LONG) {
          eat(p, TOKEN_KW_LONG);
    if (p->has_error) return (VarType){0};
          if (p->current_token.type == TOKEN_KW_LONG) {
              eat(p, TOKEN_KW_LONG);
    if (p->has_error) return (VarType){0};
              if (p->current_token.type == TOKEN_KW_DOUBLE) {
                  eat(p, TOKEN_KW_DOUBLE);
    if (p->has_error) return (VarType){0};
                  t.base = TYPE_LONG_DOUBLE;
              } else {
                  t.base = TYPE_LONG_LONG;
                  if (p->current_token.type == TOKEN_KW_INT) {
                      eat(p, TOKEN_KW_INT);
                  }
              }
          } else if (p->current_token.type == TOKEN_KW_DOUBLE) {
              eat(p, TOKEN_KW_DOUBLE);
    if (p->has_error) return (VarType){0};
              t.base = TYPE_LONG_DOUBLE;
          } else if (p->current_token.type == TOKEN_KW_INT) {
              eat(p, TOKEN_KW_INT);
    if (p->has_error) return (VarType){0};
              t.base = TYPE_LONG;
          } else {
              t.base = TYPE_LONG;
          }
      }
      else if (ct == TOKEN_KW_DOUBLE) {
          eat(p, TOKEN_KW_DOUBLE);
    if (p->has_error) return (VarType){0};
          if (p->current_token.type == TOKEN_KW_LONG) {
              eat(p, TOKEN_KW_LONG);
    if (p->has_error) return (VarType){0};
              if (p->current_token.type == TOKEN_KW_LONG) eat(p, TOKEN_KW_LONG);
              t.base = TYPE_LONG_DOUBLE;
          } else {
              t.base = TYPE_DOUBLE;
          }
      }
      else if (ct == TOKEN_KW_CHAR) { t.base = TYPE_CHAR; eat(p, TOKEN_KW_CHAR); }
      else if (ct == TOKEN_KW_BOOL || ct == TOKEN_KW_UNSIGNED || ct == TOKEN_KW_SIGNED) { t.base = TYPE_BOOL; eat(p, TOKEN_KW_BOOL); }
      else if (ct == TOKEN_KW_SINGLE) { t.base = TYPE_SINGLE; eat(p, TOKEN_KW_SINGLE); }

      else if (ct == TOKEN_KW_VOID) { t.base = TYPE_VOID; eat(p, TOKEN_KW_VOID); }
      else if (ct == TOKEN_KW_LET) { t.base = TYPE_AUTO; eat(p, TOKEN_KW_LET); }
      else if (ct == TOKEN_UNION) {
          eat(p, TOKEN_UNION);
          if (p->current_token.type == TOKEN_LBRACKET) {
              eat(p, TOKEN_LBRACKET);
              char buf[512] = "__Union_";

              ClassNode *cls = parser_alloc(p, sizeof(ClassNode));
              cls->base.type = NODE_CLASS;
              cls->is_union = 1;
              ASTNode *last_member = NULL;
              int field_idx = 0;

              while (p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF) {
                  VarType f_type = parse_type(p);
                  char *f_name = NULL;
                  if (p->current_token.type == TOKEN_IDENTIFIER) {
                      f_name = parser_strdup(p, p->current_token.text);
                      eat(p, TOKEN_IDENTIFIER);
                  } else {
                      char f_buf[32];
                      snprintf(f_buf, sizeof(f_buf), "_f%d", field_idx++);
                      f_name = parser_strdup(p, f_buf);
                  }

                  VarDeclNode *vd = parser_alloc(p, sizeof(VarDeclNode));
                  vd->base.type = NODE_VAR_DECL;
                  vd->name = f_name;
                  vd->var_type = f_type;
                  if (last_member) last_member->next = (ASTNode*)vd;
                  else cls->members = (ASTNode*)vd;
                  last_member = (ASTNode*)vd;

                  strncat(buf, f_name, sizeof(buf) - strlen(buf) - 1);
                  if (p->current_token.type == TOKEN_COMMA) {
                      strncat(buf, "_", sizeof(buf) - strlen(buf) - 1);
                      eat(p, TOKEN_COMMA);
                  }
              }
              eat(p, TOKEN_RBRACKET);
              t.base = TYPE_CLASS;
              t.class_name = parser_strdup(p, buf);
              cls->name = t.class_name;
              cls->base.next = p->synthetic_classes;
              p->synthetic_classes = (ASTNode*)cls;
          } else {
              parser_fail(p, "Expected '[' after union type");
          }
      }
      else if (ct == TOKEN_ABSTRACT) {
          eat(p, TOKEN_ABSTRACT);
          if (p->current_token.type == TOKEN_CONTAINER) {
              eat(p, TOKEN_CONTAINER);
              if (p->current_token.type == TOKEN_CLASS) {
                  eat(p, TOKEN_CLASS);
                  if (p->current_token.type == TOKEN_LBRACE) {
                      eat(p, TOKEN_LBRACE);

                      ClassNode *cls = parser_alloc(p, sizeof(ClassNode));
                      cls->base.type = NODE_CLASS;
                      ASTNode *last_member = NULL;

                       while (p->current_token.type != TOKEN_RBRACE && p->current_token.type != TOKEN_EOF) {
                           VarType f_type = parse_type(p);
                           if (p->current_token.type == TOKEN_IDENTIFIER) {
                               char *f_name = parser_strdup(p, p->current_token.text);
                               eat(p, TOKEN_IDENTIFIER);
                               VarDeclNode *vd = parser_alloc(p, sizeof(VarDeclNode));
                               vd->base.type = NODE_VAR_DECL;
                               vd->name = f_name;
                               vd->var_type = f_type;
                               if (last_member) last_member->next = (ASTNode*)vd;
                               else cls->members = (ASTNode*)vd;
                               last_member = (ASTNode*)vd;
                           }
                           if (p->current_token.type == TOKEN_SEMICOLON) eat_semi(p);
                      }
                      eat(p, TOKEN_RBRACE);
                      t.base = TYPE_CLASS;

                      // Generate a unique name using a static counter for this session
                      static int anon_struct_counter = 0;
                      char buf[64];
                      snprintf(buf, sizeof(buf), "__AnonStruct_%d", anon_struct_counter++);
                      t.class_name = parser_strdup(p, buf);
                      cls->name = t.class_name;
                      cls->base.next = p->synthetic_classes;
                      p->synthetic_classes = (ASTNode*)cls;
                  } else {
                      parser_fail(p, "Expected '{' after abstract container class");
                  }
              } else {
                  parser_fail(p, "Expected 'class' after abstract container");
              }
          } else {
              parser_fail(p, "Expected 'container' after abstract");
          }
      }
      else {
          if (t.is_unsigned) t.base = TYPE_INT;
          else return t;
      }
  }

  while (p->current_token.type == TOKEN_STAR) { if (p->has_error) break;
    t.ptr_depth++;
    eat(p, TOKEN_STAR);
    if (p->has_error) return (VarType){0};
  }

  if (p->current_token.type == TOKEN_LPAREN) {
      Token next = parser_peek_token(p);
      if (next.type == TOKEN_STAR) {
          return parse_func_ptr_decl(p, t, NULL);
    if (p->has_error) return (VarType){0};
      } else {
          Token after_parens = parser_peek_past_parens(p);
          if (after_parens.type != TOKEN_LBRACE) {
              return parse_func_sig_decl(p, t, NULL);
    if (p->has_error) return (VarType){0};
          }
      }
  }

  if (p->current_token.type == TOKEN_QUESTION) {
      t.is_tainted = 1;
      eat(p, TOKEN_QUESTION);
    if (p->has_error) return (VarType){0};
  }

  while (p->current_token.type == TOKEN_LBRACKET) { if (p->has_error) break;
      eat(p, TOKEN_LBRACKET);
      if (p->current_token.type != TOKEN_RBRACKET) {
          ASTNode *sz = parse_expression(p);
          if (sz && sz->type == NODE_LITERAL && ((LiteralNode*)sz)->var_type.base == TYPE_INT) {
              t.array_size = ((LiteralNode*)sz)->val.int_val;
          } else {
              t.array_size = 1;
          }
      } else {
          t.array_size = 1;
      }
      eat(p, TOKEN_RBRACKET);
      // t.ptr_depth++;
  }

  return t;
}

// TODO understand what the fuck is this
// This is for varshit idk wtf
/**
 * @brief Parses a function pointer type declaration.
 * @param p Parser context.
 * @param ret_type Return type of the function pointer.
 * @param out_name Optional output parameter for the function pointer name.
 * @return Parsed VarType representing the function pointer type.
 */
VarType parse_func_ptr_decl(Parser *p, VarType ret_type, char **out_name) {
    VarType vt = {0};
    vt.is_func_ptr = 1;
    vt.fp_ret_type = parser_alloc_raw(p, sizeof(VarType));
    *vt.fp_ret_type = ret_type;

    eat(p, TOKEN_LPAREN);
    if (p->has_error) return (VarType){0};
    if (p->current_token.type == TOKEN_STAR) {
        eat(p, TOKEN_STAR);
    if (p->has_error) return (VarType){0};

        if (p->current_token.type == TOKEN_IDENTIFIER) {
            if (out_name) *out_name = parser_strdup(p, p->current_token.text);
            eat(p, TOKEN_IDENTIFIER);
    if (p->has_error) return (VarType){0};
        } else if (out_name) {
            *out_name = NULL;
        }

        eat(p, TOKEN_RPAREN);
    if (p->has_error) return (VarType){0};
        eat(p, TOKEN_LPAREN);
    if (p->has_error) return (VarType){0};
    } else {
        if (out_name) *out_name = NULL;
    }

    int cap = 4;
    vt.fp_param_types = parser_alloc_raw(p, sizeof(VarType) * cap);
    vt.fp_param_count = 0;

    if (p->current_token.type != TOKEN_RPAREN) {
        while(1) {
            if (p->current_token.type == TOKEN_ELLIPSIS) {
                vt.fp_is_varargs = 1;
                eat(p, TOKEN_ELLIPSIS);
    if (p->has_error) return (VarType){0};
                break;
            }

            int pmods = parse_modifiers(p);
    if (p->has_error) return (VarType){0};
            (void)pmods; // unused in func ptr types for now
            VarType pt = parse_type(p);
    if (p->has_error) return (VarType){0};
            if (pt.base == TYPE_UNKNOWN) parser_fail(p, "Expected type in function pointer params");

            if (p->current_token.type == TOKEN_IDENTIFIER) {
                eat(p, TOKEN_IDENTIFIER);
    if (p->has_error) return (VarType){0};
            }

             if (p->current_token.type == TOKEN_LBRACKET) {
                eat(p, TOKEN_LBRACKET);
    if (p->has_error) return (VarType){0};
                if (p->current_token.type != TOKEN_RBRACKET) {
                     ASTNode* tmp = parse_expression(p);
    if (p->has_error) return (VarType){0};
                     (void)tmp;
                }
                eat(p, TOKEN_RBRACKET);
    if (p->has_error) return (VarType){0};
                pt.ptr_depth++;
            }

            if (vt.fp_param_count >= cap) {
                cap *= 2;
                VarType *new_params = parser_alloc_raw(p, sizeof(VarType) * cap);
                memcpy(new_params, vt.fp_param_types, sizeof(VarType) * vt.fp_param_count);
                vt.fp_param_types = new_params;
            }
            vt.fp_param_types[vt.fp_param_count++] = pt;

            if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA);
            else break;
        }
    }
    eat(p, TOKEN_RPAREN);
    if (p->has_error) return (VarType){0};

    return vt;
}

/**
 * @brief Parses a function signature declaration (parameter list only).
 * @param p Parser context.
 * @param ret_type Return type of the function.
 * @param out_name Optional output parameter for the function name.
 * @return Parsed VarType representing the function signature.
 */
VarType parse_func_sig_decl(Parser *p, VarType ret_type, char **out_name) {
    VarType vt = {0};
    vt.is_func_ptr = 1;
    vt.fp_ret_type = parser_alloc_raw(p, sizeof(VarType));
    *vt.fp_ret_type = ret_type;

    eat(p, TOKEN_LPAREN);
    if (p->has_error) return (VarType){0};
    if (out_name) *out_name = NULL;

    int cap = 4;
    vt.fp_param_types = parser_alloc_raw(p, sizeof(VarType) * cap);
    vt.fp_param_count = 0;

    if (p->current_token.type != TOKEN_RPAREN) {
        while(1) {
            if (p->current_token.type == TOKEN_ELLIPSIS) {
                vt.fp_is_varargs = 1;
                eat(p, TOKEN_ELLIPSIS);
                if (p->has_error) return (VarType){0};
                break;
            }

            int pmods = parse_modifiers(p);
            if (p->has_error) return (VarType){0};
            (void)pmods;
            VarType pt = parse_type(p);
            if (p->has_error) return (VarType){0};
            if (pt.base == TYPE_UNKNOWN) parser_fail(p, "Expected type in function signature params");

            if (p->current_token.type == TOKEN_IDENTIFIER) {
                eat(p, TOKEN_IDENTIFIER);
                if (p->has_error) return (VarType){0};
            }

             if (p->current_token.type == TOKEN_LBRACKET) {
                 eat(p, TOKEN_LBRACKET);
                 if (p->has_error) return (VarType){0};
                 if (p->current_token.type != TOKEN_RBRACKET) {
                      ASTNode* tmp = parse_expression(p);
                      if (p->has_error) return (VarType){0};
                      (void)tmp;
                 }
                 eat(p, TOKEN_RBRACKET);
                 if (p->has_error) return (VarType){0};
                 pt.ptr_depth++;
             }

            if (vt.fp_param_count >= cap) {
                cap *= 2;
                VarType *new_params = parser_alloc_raw(p, sizeof(VarType) * cap);
                memcpy(new_params, vt.fp_param_types, sizeof(VarType) * vt.fp_param_count);
                vt.fp_param_types = new_params;
            }
            vt.fp_param_types[vt.fp_param_count++] = pt;

            if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA);
            else break;
        }
    }
    eat(p, TOKEN_RPAREN);
    if (p->has_error) return (VarType){0};

    return vt;
}


static char* read_file_content(Parser *p, const char* path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return NULL;
    }
    FILE* f = fopen(path, "rb");
    if (!f) {
        size_t plen = strlen(path);
        char zyl_path[1024];
        if (plen + 4 < sizeof(zyl_path)) {
            memcpy(zyl_path, path, plen);
            memcpy(zyl_path + plen, ".zyl", 5);
#ifdef HAVE_LIBZIP
            return read_zip_file(zyl_path);
#endif
        }
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len <= 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char* buf = parser_alloc_raw(p, (size_t)len + 1);
    if(buf) {
        if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
            buf = NULL;
        } else {
            buf[len] = 0;
        }
    }
    fclose(f);
    return buf;
}

/**
 * @brief Reads and returns the content of an imported file, searching multiple extensions and paths.
 * @param p Parser context.
 * @param filename Name of the file to read (without extension).
 * @return File content as a null-terminated string, or NULL on failure.
 */
char* read_import_file(Parser *p, const char* filename) {
    const char *exts[] = { ".kyl", ".hky", ".alk", ".alky", ".alkyl", ".aky", ".zyl", "" };
    char path[1024];

    if (p->l && p->l->filename && p->l->filename[0]) {
        const char *file_dir = p->l->filename;
        const char *last_slash = strrchr(file_dir, '/');
        if (last_slash) {
            size_t dir_len = (size_t)(last_slash - file_dir);
            if (dir_len < sizeof(path)) {
                for (unsigned long j = 0; j < sizeof(exts)/sizeof(*exts); j++) {
                    snprintf(path, sizeof(path), "%.*s/%s%s", (int)dir_len, file_dir, filename, exts[j]);
                    char *content = read_file_content(p, path);
                    if (content) return content;
                }
            }
        }
    }

    if (p->settings.import_paths && p->settings.import_path_count > 0) {
        for (int i = 0; i < p->settings.import_path_count; i++) {
            for (unsigned long j = 0; j < sizeof(exts)/sizeof(*exts); j++) {
                snprintf(path, sizeof(path), "%s/%s%s", p->settings.import_paths[i], filename, exts[j]);
                char *content = read_file_content(p, path);
                if (content) return content;
            }
        }
    }

    const char *fallback_paths[] = { "", "lib/", "./lib/" };
    for (unsigned long i = 0; i < sizeof(fallback_paths)/sizeof(*fallback_paths); i++) {
        for (unsigned long j = 0; j < sizeof(exts)/sizeof(*exts); j++) {
            snprintf(path, sizeof(path), "%s%s%s", fallback_paths[i], filename, exts[j]);
            char *content = read_file_content(p, path);
            if (content) return content;
        }
    }
    return NULL;
}

/**
 * @brief Sets the default import search paths in the parser settings.
 * @param ps Parser settings to populate with default paths.
 */
void parser_set_default_import_paths(ParserSettings *ps) {
    static const char *paths[4];
    int count = 0;
    paths[count++] = "lib/";
    paths[count++] = "/usr/share/alkyl";
    paths[count++] = "/usr/local/share/alkyl";
    const char *home = getenv("HOME");
    if (home) {
        static char user_path[512];
        snprintf(user_path, sizeof(user_path), "%s/.local/share/alkyl", home);
        paths[count++] = user_path;
    }
    ps->import_paths = paths;
    ps->import_path_count = count;
}

/**
 * @brief Peeks at the next token without consuming it.
 * @param p Parser context.
 * @return The next token.
 */
Token parser_peek_token(Parser *p) {
    if (p->expansion_head) {
        if (p->expansion_head->pos < p->expansion_head->count) {
            return p->expansion_head->tokens[p->expansion_head->pos];
        }
    }
    if (p->tokens && p->token_pos < p->token_count) {
        return p->tokens[p->token_pos];
    }
    Token eof;
    memset(&eof, 0, sizeof(Token));
    eof.type = TOKEN_EOF;
    return eof;
}

/**
 * @brief Peeks at a token N positions ahead without consuming.
 * @param p Parser context.
 * @param offset Number of tokens ahead to peek.
 * @return The token at the given offset.
 */
Token parser_peek_token_n(Parser *p, int offset) {
    if (p->expansion_head) {
        int pos = p->expansion_head->pos + offset;
        if (pos < p->expansion_head->count) {
            return p->expansion_head->tokens[pos];
        }
    }
    if (p->tokens && p->token_pos + offset < p->token_count) {
        return p->tokens[p->token_pos + offset];
    }
    Token eof;
    memset(&eof, 0, sizeof(Token));
    eof.type = TOKEN_EOF;
    return eof;
}

static Token parser_peek_past_parens(Parser *p) {
    if (p->current_token.type != TOKEN_LPAREN) {
        return p->current_token;
    }
    
    int depth = 1;
    int start_idx;
    Token *tokens;
    int max_count;
    
    if (p->expansion_head) {
        start_idx = p->expansion_head->pos + 1;
        tokens = p->expansion_head->tokens;
        max_count = p->expansion_head->count;
    } else if (p->tokens) {
        start_idx = p->token_pos;
        tokens = p->tokens;
        max_count = p->token_count;
    } else {
        Token eof;
        memset(&eof, 0, sizeof(Token));
        eof.type = TOKEN_EOF;
        return eof;
    }
    
    int i = start_idx;
    while (i < max_count && depth > 0) {
        Token t = tokens[i++];
        if (t.type == TOKEN_LPAREN) depth++;
        else if (t.type == TOKEN_RPAREN) depth--;
    }
    
    if (i < max_count) {
        return tokens[i];
    }
    Token eof;
    memset(&eof, 0, sizeof(Token));
    eof.type = TOKEN_EOF;
    return eof;
}

/**
 * @brief Pre-scans tokens to register type names (class, struct, union, enum) without consuming.
 * @param p Parser context.
 */
void parser_prescan(Parser *p) {
    int saved_pos = p->token_pos;
    while (p->token_pos < p->token_count) {
        Token t = lexer_next_raw(p);
        if (t.type == TOKEN_EOF) break;
        if (t.type == TOKEN_CLASS || t.type == TOKEN_STRUCT || t.type == TOKEN_UNION || t.type == TOKEN_ENUM) {
            Token name = lexer_next_raw(p);
            if (name.type == TOKEN_IDENTIFIER) {
                register_typename(p, name.text, (t.type == TOKEN_ENUM));
            }
        }
    }
    p->token_pos = saved_pos;
}

/**
 * @brief Parses the entire program from the current token stream.
 * @param p Parser context.
 * @return Root AST node of the parsed program.
 */
ASTNode* parse_program(Parser *p) {
  if (p->l) {
      p->token_capacity = 1024;
      p->tokens = parser_alloc_raw(p, sizeof(Token) * p->token_capacity);
      p->token_count = 0;
      p->token_pos = 0;
      while (1) {
          Token t = lexer_next(p->l);
          if (p->token_count >= p->token_capacity) {
              int new_cap = p->token_capacity * 2;
              Token *new_tokens = parser_alloc_raw(p, sizeof(Token) * new_cap);
              memcpy(new_tokens, p->tokens, sizeof(Token) * p->token_count);
              p->tokens = new_tokens;
              p->token_capacity = new_cap;
          }
          p->tokens[p->token_count++] = t;
          if (t.type == TOKEN_EOF) break;
      }
  }

  parser_prescan(p);
  /* Expand macros on the first token so REPL lines like `t` after
   * `define t as p` resolve to the same binding as `p`. */
  p->current_token = expand_macros_from(p, lexer_next_raw(p));

  ASTNode *head = NULL;
  ASTNode **current = &head;

  while (p->current_token.type != TOKEN_EOF) {
    if (p->has_error) {
        p->has_error = 0;
        parser_sync(p);
        if (p->current_token.type == TOKEN_EOF) break;
    }

    ASTNode *node = parse_top_level(p);
    if (node) {
        if (!*current) *current = node;

        ASTNode *iter = node;
        while (iter->next) iter = iter->next;
        current = &iter->next;
    }
  }

  if (p->synthetic_classes) {
      if (!*current) *current = p->synthetic_classes;
      else {
          ASTNode *iter = head;
          while (iter->next) iter = iter->next;
          iter->next = p->synthetic_classes;
      }
  } else {
      *current = NULL;
  }

  return head;
}
