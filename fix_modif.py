with open('src/parser/modif.c', 'r') as f:
    content = f.read()

content = content.replace("if (p->recover_buf) longjmp(*p->recover_buf, 1);", "p->has_error = 1; return NULL;")

with open('src/parser/modif.c', 'w') as f:
    f.write(content)

