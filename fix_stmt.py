import re

with open("src/parser/stmt.c", "r") as f:
    content = f.read()

content = content.replace("static void eat_semi", "void eat_semi")
content = content.replace("static void set_loc", "void set_loc")

with open("src/parser/stmt.c", "w") as f:
    f.write(content)

with open("include/parser/parser_internal.h", "r") as f:
    internal = f.read()
    
internal = internal.replace("// Modularized Statement Parsers", "void eat_semi(Parser *p);\nvoid set_loc(ASTNode *n, int line, int col);\n\n// Modularized Statement Parsers")

with open("include/parser/parser_internal.h", "w") as f:
    f.write(internal)

