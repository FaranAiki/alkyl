#include "link.h"

ASTNode* parse_import_internal(Parser *p, const char *fname) {
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
       while (p->current_token.type == TOKEN_IDENTIFIER ||
              p->current_token.type == TOKEN_DOT ||
              p->current_token.type == TOKEN_SLASH) {
           if (p->current_token.type == TOKEN_DOT) {
               strcat(path_buf, "/");
           } else if (p->current_token.type == TOKEN_SLASH) {
               strcat(path_buf, "/");
           } else {
               if (p->current_token.text) {
                   strncat(path_buf, p->current_token.text, 255 - strlen(path_buf));
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
