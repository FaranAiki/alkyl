import re

with open("include/parser/typestruct.h", "r") as f:
    content = f.read()

content = content.replace("NODE_SIZEOF\n} NodeType;", "NODE_SIZEOF,\n  NODE_META,\n  NODE_POSTMETA\n} NodeType;")

node_def = """typedef struct {
  ASTNode base;
  bool is_post;
  ASTNode *body;
} MetaNode;

#endif"""

content = content.replace("#endif // PARSER_TYPESTRUCT_H", node_def + " // PARSER_TYPESTRUCT_H")

with open("include/parser/typestruct.h", "w") as f:
    f.write(content)
