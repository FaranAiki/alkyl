with open("lib/std/input.kyl", "r") as f:
    content = f.read()

new_content = content.replace(
    'if typeof(arg) == "int*" {',
    'if typeof(arg) == "int*" || typeof(arg) == "tainted int*" {'
).replace(
    '} else if typeof(arg) == "unsigned int*" {',
    '} else if typeof(arg) == "unsigned int*" || typeof(arg) == "tainted unsigned int*" {'
).replace(
    '} else if typeof(arg) == "long*" {',
    '} else if typeof(arg) == "long*" || typeof(arg) == "tainted long*" {'
).replace(
    '} else if typeof(arg) == "unsigned long*" {',
    '} else if typeof(arg) == "unsigned long*" || typeof(arg) == "tainted unsigned long*" {'
).replace(
    '} else if typeof(arg) == "single*" {',
    '} else if typeof(arg) == "single*" || typeof(arg) == "tainted single*" {'
).replace(
    '} else if typeof(arg) == "double*" {',
    '} else if typeof(arg) == "double*" || typeof(arg) == "tainted double*" {'
).replace(
    '} else if typeof(arg) == "char*" {',
    '} else if typeof(arg) == "char*" || typeof(arg) == "tainted char*" {'
).replace(
    '} else if typeof(arg) == "char**" {',
    '} else if typeof(arg) == "char**" || typeof(arg) == "tainted char**" {'
)

with open("lib/std/input.kyl", "w") as f:
    f.write(new_content)
