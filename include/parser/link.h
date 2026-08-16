#ifndef PARSER_LINK_H
#define PARSER_LINK_H

#include "parser_internal.h"

ASTNode* parse_import(Parser *p);
ASTNode* parse_import_internal(Parser *p, const char *fname);
ASTNode* parse_link(Parser *p);
void resolve_imports(Parser *p, ASTNode **root_ptr);
void add_pkg_config_flags(CompilerContext *ctx, const char *lib_name);

#endif // PARSER_LINK_H
