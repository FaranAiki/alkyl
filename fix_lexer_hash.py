import re

with open("include/lexer/lexer.h", "r") as f:
    content = f.read()

# We don't need to change lexer.h if we just use the existing `keywords` array
