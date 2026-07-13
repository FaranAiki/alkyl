with open("src/parser/fragment/class.c", "r") as f:
    content = f.read()

content = content.replace("static ASTNode* parse_class_impl", "ASTNode* parse_class_impl")

with open("src/parser/fragment/class.c", "w") as f:
    f.write(content)

with open("include/parser/parser_internal.h", "r") as f:
    internal = f.read()
    
internal = internal.replace("ASTNode* parse_class(Parser *p);", "ASTNode* parse_class(Parser *p);\nASTNode* parse_class_impl(Parser *p, int modifiers);")

with open("include/parser/parser_internal.h", "w") as f:
    f.write(internal)

