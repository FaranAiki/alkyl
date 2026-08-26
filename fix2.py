with open('src/parser/c_parser.c', 'r') as f:
    content = f.read()

old_code = """                            *inner_curr = (ASTNode*)v;
                            inner_curr = &v->base.next;"""
new_code = """                            *inner_curr = (ASTNode*)v;
                            inner_curr = &v->base.next;
                            printf("DEBUG: anonymous struct member: %s\\n", v->name);"""

content = content.replace(old_code, new_code)

with open('src/parser/c_parser.c', 'w') as f:
    f.write(content)
