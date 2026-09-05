/**
 * @file link.c
 * @brief Import resolution and linker flag handling for the Alkyl parser.
 */
#include "link.h"
#include "../parser/c_parser.h"
#include <stdio.h>
#include <string.h>

static int is_system_lib(const char *name) {
    static const char *sys_libs[] = {
        "m", "pthread", "dl", "z", "c", "rt", "nsl",
        "socket", "crypt", "resolv", "ldl", "stdc++", "ncurses",
        "X11", "Xext", "xcb", "GL", "GLU", "gtk-3", "gdk-3",
        NULL
    };
    for (int i = 0; sys_libs[i]; i++) {
        if (strcmp(name, sys_libs[i]) == 0) return 1;
    }
    return 0;
}

/**
 * @brief Appends pkg-config cflags and libs for a library to the compiler context.
 * @param ctx Compiler context to update.
 * @param lib_name Name of the library to resolve.
 */
void add_pkg_config_flags(CompilerContext *ctx, const char *lib_name) {
    char cmd[1024];
    char output[2048];
    FILE *pf;
    int got_libs = 0;

    if (!ctx || !lib_name) return;

    if (!is_system_lib(lib_name)) {
        snprintf(cmd, sizeof(cmd), "pkg-config --cflags %s 2>/dev/null", lib_name);
        pf = popen(cmd, "r");
        if (pf) {
            if (fgets(output, sizeof(output), pf)) {
                size_t len = strlen(output);
                while (len > 0 && (output[len - 1] == '\n' || output[len - 1] == '\r')) {
                    output[--len] = '\0';
                }
                if (len > 0) {
                    if (strstr(ctx->cflags, output) == NULL) {
                        if (strlen(ctx->cflags) + len + 2 < sizeof(ctx->cflags)) {
                            strcat(ctx->cflags, " ");
                            strcat(ctx->cflags, output);
                        }
                    }
                }
            }
            pclose(pf);
        }

        snprintf(cmd, sizeof(cmd), "pkg-config --libs %s 2>/dev/null", lib_name);
        pf = popen(cmd, "r");
        if (pf) {
            if (fgets(output, sizeof(output), pf)) {
                size_t len = strlen(output);
                while (len > 0 && (output[len - 1] == '\n' || output[len - 1] == '\r')) {
                    output[--len] = '\0';
                }
                if (len > 0) {
                    got_libs = 1;
                    if (strstr(ctx->link_flags, output) == NULL) {
                        if (strlen(ctx->link_flags) + len + 2 < sizeof(ctx->link_flags)) {
                            strcat(ctx->link_flags, " ");
                            strcat(ctx->link_flags, output);
                        }
                    }
                }
            }
            pclose(pf);
        }
    }

    if (!got_libs) {
        char fallback[256];
        snprintf(fallback, sizeof(fallback), "-l%s", lib_name);
        if (strstr(ctx->link_flags, fallback) == NULL) {
            if (strlen(ctx->link_flags) + strlen(fallback) + 2 < sizeof(ctx->link_flags)) {
                strcat(ctx->link_flags, " ");
                strcat(ctx->link_flags, fallback);
            }
        }
    }
}

static ASTNode* resolve_c_import(Parser *p, const char *fname) {
    debug_parser("DEBUG: link.c resolve_c_import PATH='%s'\n", fname);
    char *src = c_preprocess_header(p->ctx, fname);
    if (!src) {
        char msg[512];
        snprintf(msg, 512, "Could not preprocess C header file: '%s'", fname);
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
    }

    return c_nodes;
}

/**
 * @brief Parses and caches an imported file, sharing parser state with the parent.
 * @param p Parser context.
 * @param fname File path to import.
 * @return AST root node for the imported file, or NULL on error.
 */
ASTNode* parse_import_internal(Parser *p, const char *fname) {
   if (p->ctx) {
       if (hashmap_has(&p->ctx->import_cache, fname)) {
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

/**
 * @brief Parses an import statement, resolving the file path from a string or identifier.
 * @param p Parser context.
 * @return AST node for the import, or NULL on error.
 */
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

/**
 * @brief Parses a link statement to request a system library.
 * @param p Parser context.
 * @return AST node for the link statement, or NULL on error.
 */
ASTNode* parse_link(Parser *p) {
    eat(p, TOKEN_LINK);
    char *lib_name = NULL;
    if (p->current_token.type == TOKEN_IDENTIFIER || p->current_token.type == TOKEN_STRING || p->current_token.type == TOKEN_C_STRING) {
        lib_name = parser_strdup(p, p->current_token.text);
        p->current_token.text = NULL;
        if (p->current_token.type == TOKEN_IDENTIFIER) eat(p, TOKEN_IDENTIFIER);
        else if (p->current_token.type == TOKEN_STRING) eat(p, TOKEN_STRING);
        else eat(p, TOKEN_C_STRING);
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
        ImportExprNode *ie = (ImportExprNode*)node;
        const char *path = ie->path;
        if (path && !import_stack_contains(stack, path)) {
            import_stack_push(stack, path);
            ASTNode *resolved = NULL;
            if (ie->header == HEADER_C) {
                resolved = resolve_c_import(p, path);
            } else {
                resolved = parse_import_internal(p, path);
            }
            import_stack_pop(stack);
            if (resolved) {
                ASTNode **curr = &resolved;
                while (*curr) {
                    ASTNode *res_node = resolve_imports_node(p, *curr, stack);
                    if (res_node) *curr = res_node;
                    curr = &(*curr)->next;
                }
                ie->resolved_body = resolved;
            }
        }
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
    } else if (node->type == NODE_VAR_DECL) {
        VarDeclNode *vd = (VarDeclNode*)node;
        if (vd->initializer) {
            ASTNode *resolved = resolve_imports_node(p, vd->initializer, stack);
            if (resolved) vd->initializer = resolved;
        }
    } else if (node->type == NODE_LINK) {
        LinkNode *lnk = (LinkNode*)node;
        if (p && p->l && p->l->ctx) {
            add_pkg_config_flags(p->l->ctx, lnk->lib_name);
        }
    } else {
        // Do NOT recursively process node->next here!
        // The caller iterates through the linked list.
    }

    return node;
}

/**
 * @brief Recursively resolves all import nodes in the AST and flattens them into the root list.
 * @param p Parser context.
 * @param root_ptr Pointer to the root AST node pointer.
 */
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
