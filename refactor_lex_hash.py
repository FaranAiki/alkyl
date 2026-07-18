import re
import os

# 1. Update parser.h
parser_h_path = "include/parser/parser.h"
with open(parser_h_path, "r") as f:
    parser_h = f.read()

if "HashMap types_map;" not in parser_h:
    parser_h = parser_h.replace("#include <stdbool.h>", "#include <stdbool.h>\n#include \"../common/hashmap.h\"")
    struct_def = """    int disable_macro_expansion;
    
    Token *tokens;
    int token_count;
    int token_capacity;
    int token_pos;
    
    HashMap types_map;
    
    char *current_namespace;"""
    parser_h = parser_h.replace("    int disable_macro_expansion;\n    char *current_namespace;", struct_def)
    with open(parser_h_path, "w") as f:
        f.write(parser_h)

# 2. Update core.c
core_c_path = "src/parser/core.c"
with open(core_c_path, "r") as f:
    core_c = f.read()

# Update parser_init
parser_init_replacement = """    p->alias_head = NULL;
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
    
    p->pending_cconv = NULL;"""
core_c = core_c.replace("""    p->alias_head = NULL;
    p->expansion_head = NULL;
    p->disable_macro_expansion = 0;
    p->pending_cconv = NULL;""", parser_init_replacement)

# Update register_typename
register_typename_replacement = """void register_typename(Parser *p, const char *name, int is_enum) {
    hashmap_put(&p->types_map, name, (void*)(intptr_t)(is_enum ? 2 : 1));

    const char *current_ns = diag_get_namespace(p->ctx);
    if (current_ns && strlen(current_ns) > 0 && strcmp(current_ns, "main") != 0) {
        char full_name[512];
        snprintf(full_name, sizeof(full_name), "%s.%s", current_ns, name);
        hashmap_put(&p->types_map, parser_strdup(p, full_name), (void*)(intptr_t)(is_enum ? 2 : 1));
    }
}"""
core_c = re.sub(r'void register_typename.*?}\n', register_typename_replacement + '\n', core_c, flags=re.DOTALL)

# Update is_typename
is_typename_replacement = """int is_typename(Parser *p, const char *name) {
    return hashmap_has(&p->types_map, name);
}"""
core_c = re.sub(r'int is_typename.*?return 0;\n}', is_typename_replacement, core_c, flags=re.DOTALL)

# Update get_typename_kind
get_typename_kind_replacement = """static int get_typename_kind(Parser *p, const char *name) {
    if (hashmap_has(&p->types_map, name)) {
        return (int)(intptr_t)hashmap_get(&p->types_map, name);
    }
    return 0;
}"""
core_c = re.sub(r'static int get_typename_kind.*?return 0;\n}', get_typename_kind_replacement, core_c, flags=re.DOTALL)

# Update lexer_next_raw
lexer_next_raw_replacement = """Token lexer_next_raw(Parser *p) {
    if (p->tokens && p->token_pos < p->token_count) {
        return p->tokens[p->token_pos++];
    }
    Token eof;
    memset(&eof, 0, sizeof(Token));
    eof.type = TOKEN_EOF;
    return eof;
}"""
core_c = re.sub(r'Token lexer_next_raw.*?}\n', lexer_next_raw_replacement + '\n', core_c, flags=re.DOTALL)

# Update parser_peek_token
parser_peek_token_replacement = """Token parser_peek_token(Parser *p) {
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
}"""
core_c = re.sub(r'Token parser_peek_token.*?}\n', parser_peek_token_replacement + '\n', core_c, flags=re.DOTALL)

# Update parser_prescan
parser_prescan_replacement = """void parser_prescan(Parser *p) {
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
}"""
core_c = re.sub(r'void parser_prescan.*?}\n', parser_prescan_replacement + '\n', core_c, flags=re.DOTALL)

# Update parse_program
parse_program_replacement = """ASTNode* parse_program(Parser *p) {
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
  p->current_token = lexer_next_raw(p);"""
core_c = core_c.replace("""ASTNode* parse_program(Parser *p) {
  parser_prescan(p);
  p->current_token = lexer_next_raw(p);""", parse_program_replacement)

# Update parse_type Lexer saved_l
core_c = core_c.replace("Lexer saved_l = *(p->l);", "int saved_pos = p->token_pos;")
core_c = core_c.replace("*(p->l) = saved_l;", "p->token_pos = saved_pos;")

with open(core_c_path, "w") as f:
    f.write(core_c)

# 3. Update link.c
link_c_path = "src/parser/link.c"
with open(link_c_path, "r") as f:
    link_c = f.read()

link_c = link_c.replace("import_p.type_head = p->type_head;", "import_p.types_map = p->types_map;")
link_c = link_c.replace("p->type_head = import_p.type_head;", "p->types_map = import_p.types_map;")

with open(link_c_path, "w") as f:
    f.write(link_c)

print("Done phase 1 and 2")
