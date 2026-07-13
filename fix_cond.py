with open("src/parser/fragment/cond.c", "r") as f:
    content = f.read()

# We can just add a forward declaration at the top
decl = "static ASTNode* parse_case_body_stmts(Parser *p);\n"

if "static ASTNode* parse_case_body_stmts(Parser *p);" not in content:
    content = content.replace("#include <stdio.h>\n\n", "#include <stdio.h>\n\n" + decl)
    
with open("src/parser/fragment/cond.c", "w") as f:
    f.write(content)

