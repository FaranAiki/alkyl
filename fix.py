import sys

with open('src/parser/c_parser.c', 'r') as f:
    content = f.read()

# Fix c_eat
old_eat = """static void c_eat(CParser *p, CTokenType expected) {
    if (p->current.type == expected) {
        p->current = c_lexer_next(&p->lexer);
    } else {
        if (!p->has_error) {
            char t1[64], t2[64];
            c_get_token_string(expected, t1, sizeof(t1));
            c_get_token_string(p->current.type, t2, sizeof(t2));
            diag_err(p->ctx, p->current.line, p->current.col, "Expected '%s' but found %s", t1, t2);
            p->has_error = 1;
        }
    }
}"""
new_eat = """static void c_eat(CParser *p, CTokenType expected) {
    if (p->current.type == expected) {
        p->current = c_lexer_next(&p->lexer);
    } else {
        if (!p->has_error) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Expected %s but found %s ('%s')",
                     c_token_type_to_string(expected),
                     c_token_type_to_string(p->current.type),
                     p->current.text ? p->current.text : "");
            c_parser_error(p, buf);
            p->current = c_lexer_next(&p->lexer);
        }
    }
}"""
content = content.replace(old_eat, new_eat)

# Fix c_parse_struct_or_union bracket skipping
old_bracket = """                    while (c_match(p, C_TOKEN_LBRACKET)) {
                        c_eat(p, C_TOKEN_LBRACKET);
                        if (c_match(p, C_TOKEN_NUMBER)) c_eat(p, C_TOKEN_NUMBER);
                        else if (c_match(p, C_TOKEN_IDENTIFIER)) c_eat(p, C_TOKEN_IDENTIFIER);
                        c_eat(p, C_TOKEN_RBRACKET);
                    }"""
new_bracket = """                    while (c_match(p, C_TOKEN_LBRACKET)) {
                        c_eat(p, C_TOKEN_LBRACKET);
                        int depth = 1;
                        while (depth > 0 && !p->has_error) {
                            if (c_match(p, C_TOKEN_LBRACKET)) { depth++; c_eat(p, C_TOKEN_LBRACKET); }
                            else if (c_match(p, C_TOKEN_RBRACKET)) { depth--; c_eat(p, C_TOKEN_RBRACKET); }
                            else if (c_match(p, C_TOKEN_SEMICOLON) || c_match(p, C_TOKEN_EOF)) { break; }
                            else { c_eat(p, p->current.type); }
                        }
                    }"""
content = content.replace(old_bracket, new_bracket)

# Fix standalone enum in c_parse_declaration
old_decl = """    if (c_match(p, C_TOKEN_STRUCT) || c_match(p, C_TOKEN_UNION) ||
        (c_match(p, C_TOKEN_IDENTIFIER) && (streq_lit(p->current.text, "class") || streq_lit(p->current.text, "struct") || streq_lit(p->current.text, "union")))) {
        return c_parse_struct_or_union(p, c_match(p, C_TOKEN_UNION));
    }

    if (c_match(p, C_TOKEN_ENUM)) {
        return c_parse_enum(p);
    }"""
new_decl = """    if (c_match(p, C_TOKEN_STRUCT) || c_match(p, C_TOKEN_UNION) || c_match(p, C_TOKEN_ENUM) ||
        (c_match(p, C_TOKEN_IDENTIFIER) && (streq_lit(p->current.text, "class") || streq_lit(p->current.text, "struct") || streq_lit(p->current.text, "union")))) {
        
        CLexer look_l = p->lexer;
        CToken t2 = c_lexer_next(&look_l);
        int is_standalone = 0;
        
        if (t2.type == C_TOKEN_LBRACE || t2.type == C_TOKEN_COLON || t2.type == C_TOKEN_ATTRIBUTE) {
            is_standalone = 1;
        } else if (t2.type == C_TOKEN_IDENTIFIER) {
            CToken t3 = c_lexer_next(&look_l);
            if (t3.type == C_TOKEN_LBRACE || t3.type == C_TOKEN_SEMICOLON || t3.type == C_TOKEN_COLON || t3.type == C_TOKEN_ATTRIBUTE) {
                is_standalone = 1;
            }
        }
        
        if (is_standalone) {
            if (c_match(p, C_TOKEN_ENUM)) {
                return c_parse_enum(p);
            } else {
                return c_parse_struct_or_union(p, c_match(p, C_TOKEN_UNION));
            }
        }
    }"""
content = content.replace(old_decl, new_decl)

# Fix c_parse_typedef standalone enum/struct check
old_typedef = """    if (c_match(p, C_TOKEN_ENUM)) {
        return c_parse_enum(p);
    }
    if (c_match(p, C_TOKEN_STRUCT) || c_match(p, C_TOKEN_UNION)) {
        int is_union = c_match(p, C_TOKEN_UNION);
        c_eat(p, p->current.type);

        char *tag_name = NULL;"""
new_typedef = """    CLexer look_l = p->lexer;
    CToken t2 = c_lexer_next(&look_l);
    int is_standalone = 0;
    
    if (c_match(p, C_TOKEN_ENUM) || c_match(p, C_TOKEN_STRUCT) || c_match(p, C_TOKEN_UNION)) {
        if (t2.type == C_TOKEN_LBRACE || t2.type == C_TOKEN_COLON || t2.type == C_TOKEN_ATTRIBUTE) {
            is_standalone = 1;
        } else if (t2.type == C_TOKEN_IDENTIFIER) {
            CToken t3 = c_lexer_next(&look_l);
            if (t3.type == C_TOKEN_LBRACE || t3.type == C_TOKEN_SEMICOLON || t3.type == C_TOKEN_COLON || t3.type == C_TOKEN_ATTRIBUTE) {
                is_standalone = 1;
            }
        }
    }
    
    if (is_standalone) {
        if (c_match(p, C_TOKEN_ENUM)) {
            return c_parse_enum(p);
        }
    }
    
    if (!is_standalone && c_match(p, C_TOKEN_ENUM)) {
        // Not standalone enum, let it fall through to c_parse_c_type
    } else if (is_standalone && (c_match(p, C_TOKEN_STRUCT) || c_match(p, C_TOKEN_UNION))) {
        int is_union = c_match(p, C_TOKEN_UNION);
        c_eat(p, p->current.type);

        char *tag_name = NULL;"""
content = content.replace(old_typedef, new_typedef)

with open('src/parser/c_parser.c', 'w') as f:
    f.write(content)

with open('src/parser/c_parser.c', 'a') as f:
    f.write("""
char* c_preprocess_header(CompilerContext *ctx, const char *fname) {
    char cmd[1024];
    if (fname[0] == '/') {
        snprintf(cmd, sizeof(cmd), "echo '#include \\"%s\\"' | gcc -E -DWLR_USE_UNSTABLE -xc - 2>/dev/null", fname);
    } else {
        snprintf(cmd, sizeof(cmd), "echo '#include <%s>' | gcc -E -DWLR_USE_UNSTABLE -I. -xc - 2>/dev/null", fname);
    }

    FILE *f = popen(cmd, "r");
    if (!f) return NULL;

    size_t cap = 16384;
    size_t len = 0;
    char *buf = malloc(cap);

    while (1) {
        size_t bytes = fread(buf + len, 1, cap - len - 1, f);
        if (bytes == 0) break;
        len += bytes;
        if (len >= cap - 1) {
            cap *= 2;
            char *new_buf = realloc(buf, cap);
            buf = new_buf;
        }
    }
    buf[len] = '\\0';
    pclose(f);

    if (len == 0) {
        free(buf);
        return NULL;
    }

    char *arena_buf = arena_alloc(ctx->arena, len + 1);
    if (arena_buf) memcpy(arena_buf, buf, len + 1);
    free(buf);

    return arena_buf;
}
""")
