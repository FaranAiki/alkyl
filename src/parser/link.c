#include "link.h"
#include "../parser/c_parser.h"

static ASTNode* resolve_c_import(Parser *p, const char *fname) {
    if (p->ctx) {
        ASTNode *cached = (ASTNode*)hashmap_get(&p->ctx->import_cache, fname);
        if (cached) return NULL;
    }

    char *src = read_import_file(p, fname);
    if (!src) {
        char msg[512];
        snprintf(msg, 512, "Could not open C header file: '%s'", fname);
        parser_fail(p, msg);
        return NULL;
    }

    CParser cp;
    c_parser_init(&cp, p->ctx, fname, src);
    ASTNode *c_nodes = c_parse_header(&cp);

    if (p->ctx) {
        if (!c_nodes) {
            c_nodes = parser_alloc(p, sizeof(ASTNode));
            c_nodes->type = NODE_ROOT;
        }
        hashmap_put(&p->ctx->import_cache, fname, c_nodes);
    }

    return c_nodes;
}

ASTNode* parse_import_internal(Parser *p, const char *fname) {
   if (p->ctx) {
       ASTNode *cached = (ASTNode*)hashmap_get(&p->ctx->import_cache, fname);
       if (cached) {
           return NULL;
       }
   }

   char* src = read_import_file(p, fname);
   if (!src) {
       char msg[512];
       snprintf(msg, 512, "Could not open imported file: '%s'", fname);
       parser_fail(p, msg);
       return NULL;
   }

   Lexer import_l;
   lexer_init(&import_l, p->ctx, fname, src, NULL);

   Parser import_p;
   parser_init(&import_p, &import_l, &p->settings);

   // Share state to allow macros, typedefs, and struct types to cross file boundaries
   import_p.macro_head = p->macro_head;
   import_p.type_head = p->type_head;
   import_p.types_map = p->types_map;
   import_p.alias_head = p->alias_head;

   ASTNode* imported_root = parse_program(&import_p);

   // Bring the global definitions back into the parent parser's scope
   p->macro_head = import_p.macro_head;
   p->type_head = import_p.type_head;
   p->types_map = import_p.types_map;
   p->alias_head = import_p.alias_head;

   if (p->ctx) {
       if (!imported_root) {
           imported_root = parser_alloc(p, sizeof(ASTNode));
           imported_root->type = NODE_ROOT;
       }
       hashmap_put(&p->ctx->import_cache, fname, imported_root);
   }

   return imported_root;
}

ASTNode* parse_import(Parser *p) {
   eat(p, TOKEN_IMPORT);
   if (p->has_error) return NULL;
   char* fname = NULL;
   if (p->current_token.type == TOKEN_STRING || p->current_token.type == TOKEN_C_STRING) {
       fname = parser_strdup(p, p->current_token.text);
       p->current_token.text = NULL;
       eat(p, p->current_token.type);
   } else {
       if (p->l->settings.import_require_double_quotes) {
           parser_fail(p, "Expected file path string after 'import'");
           return NULL;
       }
        char path_buf[256] = {0};
        size_t path_len = 0;
        while (p->current_token.type == TOKEN_IDENTIFIER ||
               p->current_token.type == TOKEN_DOT ||
               p->current_token.type == TOKEN_SLASH) {
            if (p->current_token.type == TOKEN_DOT) {
                if (path_len + 1 < sizeof(path_buf)) {
                    path_buf[path_len++] = '/';
                    path_buf[path_len] = '\0';
                }
            } else if (p->current_token.type == TOKEN_SLASH) {
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
       if (strlen(path_buf) == 0) {
           parser_fail(p, "Expected file path after 'import'");
           return NULL;
       }
       fname = parser_strdup(p, path_buf);
   }

   // optional semicolon
   if (p->current_token.type == TOKEN_SEMICOLON) {
       eat_semi(p);
   }

   ImportNode *node = parser_alloc(p, sizeof(ImportNode));
   node->base.type = NODE_IMPORT;
   node->path = fname;
   node->resolved_body = NULL;
   set_loc((ASTNode*)node, p->current_token.line, p->current_token.col);
   return (ASTNode*)node;
}

ASTNode* parse_link(Parser *p) {
  eat(p, TOKEN_LINK);
  char *lib_name = NULL;
  if (p->current_token.type == TOKEN_IDENTIFIER || p->current_token.type == TOKEN_STRING) {
    lib_name = parser_strdup(p, p->current_token.text);
    p->current_token.text = NULL;
    if (p->current_token.type == TOKEN_IDENTIFIER) eat(p, TOKEN_IDENTIFIER);
    else eat(p, TOKEN_STRING);
  } else {
    parser_fail(p, "Expected library name (string or identifier) after 'link'");
    return NULL;
  }
  if (p->current_token.type == TOKEN_SEMICOLON) eat_semi(p);
  LinkNode *node = parser_alloc(p, sizeof(LinkNode));
  node->base.type = NODE_LINK;
  node->lib_name = lib_name;
  return (ASTNode*)node;
}

typedef struct {
    const char **paths;
    int count;
    int capacity;
} ImportStack;

static int import_stack_contains(ImportStack *stack, const char *path) {
    for (int i = 0; i < stack->count; i++) {
        if (streq_lit(stack->paths[i], path)) return 1;
    }
    return 0;
}

static void import_stack_push(ImportStack *stack, const char *path) {
    if (stack->count >= stack->capacity) {
        stack->capacity = stack->capacity == 0 ? 16 : stack->capacity * 2;
        const char **tmp = (const char**)realloc(stack->paths, stack->capacity * sizeof(char*));
        if (!tmp) return;
        stack->paths = tmp;
    }
    stack->paths[stack->count++] = path;
}

static void import_stack_pop(ImportStack *stack) {
    if (stack->count > 0) stack->count--;
}

static ASTNode* resolve_imports_node(Parser *p, ASTNode *node, ImportStack *stack) {
    if (!node) return NULL;
    
    if (node->type == NODE_IMPORT) {
        ImportNode *in = (ImportNode*)node;
        const char *path = in->path;
        
        if (import_stack_contains(stack, path)) {
            return NULL;
        }
        
        import_stack_push(stack, path);
        
        ASTNode *resolved = NULL;
        if (in->header == HEADER_C) {
            resolved = resolve_c_import(p, path);
        } else {
            resolved = parse_import_internal(p, path);
        }
        
        import_stack_pop(stack);
        
        if (resolved) {
            ASTNode **curr = &resolved;
            while (*curr) {
                ASTNode *res_node = resolve_imports_node(p, *curr, stack);
                if (res_node) {
                    *curr = res_node;
                }
                curr = &(*curr)->next;
            }
        }
        
        in->resolved_body = resolved;
        
        if (resolved) {
            ASTNode *last = resolved;
            while (last->next) last = last->next;
            last->next = node->next;
        }
        
        return resolved;
    }
    
    if (node->type == NODE_IMPORT_EXPR) {
        return node;
    }
    
    if (node->type == NODE_NAMESPACE) {
        ASTNode **curr = &((NamespaceNode*)node)->body;
        while (*curr) {
            ASTNode *resolved = resolve_imports_node(p, *curr, stack);
            if (resolved) *curr = resolved;
            curr = &(*curr)->next;
        }
    } else if (node->type == NODE_CLASS) {
        ASTNode **curr = &((ClassNode*)node)->members;
        while (*curr) {
            ASTNode *resolved = resolve_imports_node(p, *curr, stack);
            if (resolved) *curr = resolved;
            curr = &(*curr)->next;
        }
    } else if (node->type == NODE_FUNC_DEF) {
        ASTNode **curr = &((FuncDefNode*)node)->body;
        while (*curr) {
            ASTNode *resolved = resolve_imports_node(p, *curr, stack);
            if (resolved) *curr = resolved;
            curr = &(*curr)->next;
        }
    } else if (node->type == NODE_COMPOUND) {
        ASTNode **curr = &((CompoundNode*)node)->body;
        while (*curr) {
            ASTNode *resolved = resolve_imports_node(p, *curr, stack);
            if (resolved) *curr = resolved;
            curr = &(*curr)->next;
        }
    } else {
        // Do NOT recursively process node->next here!
        // The caller iterates through the linked list.
    }
    
    return node;
}

void resolve_imports(Parser *p, ASTNode **root_ptr) {
    if (!root_ptr || !*root_ptr) return;
    
    ImportStack stack = {0};
    
    ASTNode **curr = root_ptr;
    while (*curr) {
        ASTNode *resolved = resolve_imports_node(p, *curr, &stack);
        if (resolved) *curr = resolved;
        curr = &(*curr)->next;
    }
    
    free(stack.paths);
}
