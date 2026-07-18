#include "parser_internal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
    
    if (p->ctx && p->ctx->arena) {
        hashmap_init(&p->types_map, p->ctx->arena, 64);
    } else {
        hashmap_init(&p->types_map, NULL, 64);
    }
    
    p->pending_cconv = NULL;
    
    if (settings) {
        p->settings = *settings;
    } else {
        // Defaults
        p->settings.require_parens_for_conditions = 0;
        p->settings.allow_implicit_return = 0;
        p->settings.allow_postfix_types = 0;
        p->settings.strict_boolean_conditions = 0;
    }
    
    if (l) {
        p->current_token.type = TOKEN_UNKNOWN;
    }
}

void* parser_alloc(Parser *p, size_t size) {
    if (!p || !p->ctx || !p->ctx->arena) return calloc(1, size);
    void *ptr = arena_alloc(p->ctx->arena, size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

char* parser_strdup(Parser *p, const char *str) {
    if (!str) return NULL;
    if (!p || !p->ctx || !p->ctx->arena) return strdup(str); 
    return arena_strdup(p->ctx->arena, str);
}

void register_typename(Parser *p, const char *name, int is_enum) {
    hashmap_put(&p->types_map, name, (void*)(intptr_t)(is_enum ? 2 : 1));

    const char *current_ns = diag_get_namespace(p->ctx);
    if (current_ns && strlen(current_ns) > 0 && strcmp(current_ns, "main") != 0) {
        char full_name[512];
        snprintf(full_name, sizeof(full_name), "%s.%s", current_ns, name);
        hashmap_put(&p->types_map, parser_strdup(p, full_name), (void*)(intptr_t)(is_enum ? 2 : 1));
    }
}

int is_typename(Parser *p, const char *name) {
    return hashmap_has(&p->types_map, name);
}

int is_type_start(Parser *p) {
    TokenType ct = p->current_token.type;
    if (ct == TOKEN_KW_INT || ct == TOKEN_KW_SHORT || ct == TOKEN_KW_LONG || 
        ct == TOKEN_KW_DOUBLE || ct == TOKEN_KW_SINGLE || ct == TOKEN_KW_CHAR || 
        ct == TOKEN_KW_VOID || ct == TOKEN_KW_BOOL) {
        return 1;
    }
    if (ct == TOKEN_IDENTIFIER && is_typename(p, p->current_token.text)) {
        return 1;
    }
    return 0;
}

static int get_typename_kind(Parser *p, const char *name) {
    if (hashmap_has(&p->types_map, name)) {
        return (int)(intptr_t)hashmap_get(&p->types_map, name);
    }
    return 0;
}

void register_alias(Parser *p, const char *name, VarType target) {
    TypeAlias *curr = p->alias_head;
    while(curr) {
        if (strcmp(curr->name, name) == 0) {
            curr->target = target;
            return;
        }
        curr = curr->next;
    }

    TypeAlias *a = parser_alloc(p, sizeof(TypeAlias));
    a->name = parser_strdup(p, name);
    a->target = target;
    if (target.class_name) a->target.class_name = parser_strdup(p, target.class_name);
    
    a->next = p->alias_head;
    p->alias_head = a;
}

VarType* get_alias(Parser *p, const char *name) {
    TypeAlias *curr = p->alias_head;
    while(curr) {
        if (strcmp(curr->name, name) == 0) return &curr->target;
        curr = curr->next;
    }
    return NULL;
}

Token token_clone(Parser *p, Token t) {
    Token new_t = t;
    if (t.text) new_t.text = parser_strdup(p, t.text);
    return new_t;
}

void register_macro(Parser *p, const char *name, char **params, int param_count, Token *body, int body_len) {
    Macro *m = parser_alloc(p, sizeof(Macro));
    m->name = parser_strdup(p, name);
    m->params = params; 
    m->param_count = param_count;
    m->body = parser_alloc(p, sizeof(Token) * body_len);
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
        if (strcmp(curr->name, name) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

Token lexer_next_raw(Parser *p) {
    if (p->tokens && p->token_pos < p->token_count) {
        return p->tokens[p->token_pos++];
    }
    Token eof;
    memset(&eof, 0, sizeof(Token));
    eof.type = TOKEN_EOF;
    return eof;
}

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

void parser_fail_at(Parser *p, Token t, const char *msg) {
    report_error(p->l, t, msg); 
    if (p->ctx) p->ctx->error_count++;
    p->has_error = 1;
}

void parser_fail(Parser *p, const char *msg) {
    parser_fail_at(p, p->current_token, msg);
}

void parser_sync(Parser *p) {
    while (p->current_token.type != TOKEN_EOF) {
        if (p->current_token.type == TOKEN_SEMICOLON) {
            eat(p, TOKEN_SEMICOLON);
    if (p->has_error) return;
            return;
        }
        if (p->current_token.type == TOKEN_RBRACE) {
            eat(p, TOKEN_RBRACE);
    if (p->has_error) return;
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
    if (p->has_error) return;
        }
    }
}

void eat(Parser *p, TokenType type) {
  if (p->has_error) return;
  if (p->current_token.type == type) {
    Token t = fetch_safe(p);
    
    while (!p->disable_macro_expansion && t.type == TOKEN_IDENTIFIER) {
        Macro *m = find_macro(p, t.text);
        if (!m) break; 
        
        Token **args = NULL;
        int *arg_lens = NULL;
        
        if (m->param_count > 0) {
            Token peek = fetch_safe(p);
            if (peek.type != TOKEN_LPAREN) {
                parser_fail(p, "Function-like macro requires arguments list '('.");
            }

            args = parser_alloc(p, sizeof(Token*) * m->param_count);
            arg_lens = parser_alloc(p, m->param_count * sizeof(int));
            
            for(int i=0; i<m->param_count; i++) {
                int cap = 8; int len = 0;
                args[i] = parser_alloc(p, sizeof(Token) * cap);
                int depth = 0;
                while(1) {
                    Token arg_t = fetch_safe(p);
                    if (arg_t.type == TOKEN_EOF) parser_fail(p, "Unexpected EOF in macro arguments");
                    
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
                        Token *new_arr = parser_alloc(p, sizeof(Token)*cap);
                        memcpy(new_arr, args[i], sizeof(Token)*len);
                        args[i] = new_arr;
                    }
                    args[i][len++] = arg_t;
                }
                arg_lens[i] = len;
            }
        }
        
        int res_cap = m->body_len * 2 + 16;
        int res_len = 0;
        Token *res = parser_alloc(p, sizeof(Token) * res_cap);
        
        for(int i=0; i<m->body_len; i++) {
            Token bt = m->body[i];
            int p_idx = -1;
            if (bt.type == TOKEN_IDENTIFIER && m->param_count > 0) {
                for(int k=0; k<m->param_count; k++) {
                    if (strcmp(bt.text, m->params[k]) == 0) { p_idx = k; break; }
                }
            }
            
            if (p_idx != -1) {
                for(int k=0; k<arg_lens[p_idx]; k++) {
                    if (res_len >= res_cap) { 
                        res_cap *= 2; 
                        Token *new_res = parser_alloc(p, sizeof(Token)*res_cap);
                        memcpy(new_res, res, sizeof(Token)*res_len);
                        res = new_res;
                    }
                    res[res_len++] = token_clone(p, args[p_idx][k]);
                }
            } else {
                if (res_len >= res_cap) { 
                    res_cap *= 2; 
                    Token *new_res = parser_alloc(p, sizeof(Token)*res_cap);
                    memcpy(new_res, res, sizeof(Token)*res_len);
                    res = new_res;
                }
                res[res_len++] = token_clone(p, bt);
            }
        }
        
        Expansion *ex = parser_alloc(p, sizeof(Expansion));
        ex->tokens = res;
        ex->count = res_len;
        ex->pos = 0;
        ex->next = p->expansion_head;
        p->expansion_head = ex;
        
        t = fetch_safe(p);
    }
    
    p->current_token = t;

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
VarType parse_type(Parser *p) {
  VarType t = {0}; 
  t.base = TYPE_UNKNOWN;



  if (p->current_token.type == TOKEN_KW_UNSIGNED) {
      t.is_unsigned = 1;
      eat(p, TOKEN_KW_UNSIGNED);
    if (p->has_error) return (VarType){0};
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

          char full_type_name[512];
          snprintf(full_type_name, sizeof(full_type_name), "%s", p->current_token.text);
          eat(p, TOKEN_IDENTIFIER);
    if (p->has_error) return (VarType){0};
          
          while (p->current_token.type == TOKEN_DOT) {
              eat(p, TOKEN_DOT);
    if (p->has_error) return (VarType){0};
              strcat(full_type_name, ".");
              if (p->current_token.type == TOKEN_IDENTIFIER) {
                  strcat(full_type_name, p->current_token.text);
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
                  
                  if (p->current_token.type == TOKEN_LBRACKET) {
                      char full_name[1024];
                      snprintf(full_name, sizeof(full_name), "%s", base_name);
                      
                      eat(p, TOKEN_LBRACKET);
    if (p->has_error) return (VarType){0};
                      strcat(full_name, "[");
                      
                      while (p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF) {
                          if (p->current_token.text) {
                              strcat(full_name, p->current_token.text);
                          } else {
                              strcat(full_name, token_type_to_string(p->current_token.type));
                          }
                          eat(p, p->current_token.type);
    if (p->has_error) return (VarType){0};
                      }
                      eat(p, TOKEN_RBRACKET);
    if (p->has_error) return (VarType){0};
                      strcat(full_name, "]");
                      t.class_name = parser_strdup(p, full_name);
                  } else {
                      t.class_name = parser_strdup(p, base_name);
                  }
              }
          } else {
              p->token_pos = saved_pos;
              p->current_token = saved_tok;
              if (t.is_unsigned) t.base = TYPE_INT;
              return t; 
          }
      }
  } else {
      TokenType ct = p->current_token.type;
      if (ct == TOKEN_KW_INT) { t.base = TYPE_INT; eat(p, TOKEN_KW_INT); }
      else if (ct == TOKEN_KW_SHORT) { t.base = TYPE_SHORT; eat(p, TOKEN_KW_SHORT); }
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
      else if (ct == TOKEN_KW_BOOL) { t.base = TYPE_BOOL; eat(p, TOKEN_KW_BOOL); }
      else if (ct == TOKEN_KW_SINGLE) { t.base = TYPE_FLOAT; eat(p, TOKEN_KW_SINGLE); }

      else if (ct == TOKEN_KW_VOID) { t.base = TYPE_VOID; eat(p, TOKEN_KW_VOID); }
      else if (ct == TOKEN_KW_LET) { t.base = TYPE_AUTO; eat(p, TOKEN_KW_LET); }
      else {
          if (t.is_unsigned) t.base = TYPE_INT; 
          else return t; 
      }
  }

  while (p->current_token.type == TOKEN_STAR) {
    t.ptr_depth++;
    eat(p, TOKEN_STAR);
    if (p->has_error) return (VarType){0};
  }
  
  if (p->current_token.type == TOKEN_LPAREN) {
      Token next = parser_peek_token(p);
      if (next.type == TOKEN_STAR) {
          return parse_func_ptr_decl(p, t, NULL);
    if (p->has_error) return (VarType){0};
      }
  }
  
  if (p->current_token.type == TOKEN_QUESTION) {
      t.is_tainted = 1;
      eat(p, TOKEN_QUESTION);
    if (p->has_error) return (VarType){0};
  }

  return t;
}

// TODO understand what the fuck is this
// This is for varshit idk wtf
VarType parse_func_ptr_decl(Parser *p, VarType ret_type, char **out_name) {
    VarType vt = {0};
    vt.is_func_ptr = 1;
    vt.fp_ret_type = parser_alloc(p, sizeof(VarType));
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
    vt.fp_param_types = parser_alloc(p, sizeof(VarType) * cap);
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
                VarType *new_params = parser_alloc(p, sizeof(VarType) * cap);
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
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = parser_alloc(p, len + 1);
    if(buf) { fread(buf, 1, len, f); buf[len] = 0; }
    fclose(f);
    return buf;
}

char* read_import_file(Parser *p, const char* filename) {
  const char* paths[] = { "", "lib/" };
  const char* exts[] = { ".aky", ".hky", ".alk", ".alky", ".alkyl", "" };
  char path[1024];
  
  for (unsigned long i = 0; i < sizeof(paths)/sizeof(*paths); i++) {
      for (unsigned long j = 0; j < sizeof(exts)/sizeof(*exts); j++) {
          snprintf(path, sizeof(path), "%s%s%s", paths[i], filename, exts[j]);
          char *content = read_file_content(p, path);
          if (content) return content;
      }
  }
  return NULL;
}

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

ASTNode* parse_program(Parser *p) {
  if (p->l) {
      p->token_capacity = 1024;
      p->tokens = parser_alloc(p, sizeof(Token) * p->token_capacity);
      p->token_count = 0;
      p->token_pos = 0;
      while (1) {
          Token t = lexer_next(p->l);
          if (p->token_count >= p->token_capacity) {
              int new_cap = p->token_capacity * 2;
              Token *new_tokens = parser_alloc(p, sizeof(Token) * new_cap);
              memcpy(new_tokens, p->tokens, sizeof(Token) * p->token_count);
              p->tokens = new_tokens;
              p->token_capacity = new_cap;
          }
          p->tokens[p->token_count++] = t;
          if (t.type == TOKEN_EOF) break;
      }
  }

  parser_prescan(p);
  p->current_token = lexer_next_raw(p);
  
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
  
  *current = NULL;
  return head;
}