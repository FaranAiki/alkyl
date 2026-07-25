import re

with open('src/alir/lvalue.c', 'r') as f:
    code = f.read()

# I need to modify NODE_TYPEOF to create a global variable representing TypeInfo
# TypeInfo structure: { unsigned int id; char* name; unsigned long size; }
# ALIR doesn't easily support struct literal generation inline without a bunch of alir_alloc, mk_inst, etc.
# Wait, let's see how ALIR generates a struct literal.
