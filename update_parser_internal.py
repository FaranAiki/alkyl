with open("include/parser/parser_internal.h", "r") as f:
    content = f.read()

funcs = [
    "ASTNode* parse_enum(Parser *p);",
    "ASTNode* parse_class(Parser *p);"
]
insert_idx = content.find("// Modularized Statement Parsers")
if insert_idx != -1:
    content = content[:insert_idx] + "\n".join(funcs) + "\n" + content[insert_idx:]
    with open("include/parser/parser_internal.h", "w") as f:
        f.write(content)
