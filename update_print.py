import re

with open('lib/std/print.aky', 'r') as f:
    code = f.read()

new_cases = """        } else if typeof(arg) == "unsigned int" {
            clib.printf("%u", arg);
        } else if typeof(arg) == "long" {
            clib.printf("%ld", arg);
        } else if typeof(arg) == "unsigned long" {
            clib.printf("%lu", arg);
        } else if typeof(arg) == "long long" {
            clib.printf("%lld", arg);
        } else if typeof(arg) == "unsigned long long" {
            clib.printf("%llu", arg);
        } else if typeof(arg) == "single" {
            clib.printf("%f", arg);
        } else if typeof(arg) == "double" {
            clib.printf("%lf", arg);
        } else if typeof(arg) == "char" {
            clib.printf("%c", arg);
        } else if typeof(arg) == "int[]" {
            clib.printf("[");
            for i in (sizeof(arg) / sizeof(int) - 1) {
                clib.printf("%d, ", arg[i]);
            }
            clib.printf("%d]", arg[sizeof(arg) / sizeof(int) - 1]);
        } else if typeof(arg) == "unsigned int[]" {
            clib.printf("[");
            for i in (sizeof(arg) / sizeof(unsigned int) - 1) {
                clib.printf("%u, ", arg[i]);
            }
            clib.printf("%u]", arg[sizeof(arg) / sizeof(unsigned int) - 1]);
        } else if typeof(arg) == "long[]" {
            clib.printf("[");
            for i in (sizeof(arg) / sizeof(long) - 1) {
                clib.printf("%ld, ", arg[i]);
            }
            clib.printf("%ld]", arg[sizeof(arg) / sizeof(long) - 1]);
        } else if typeof(arg) == "unsigned long[]" {
            clib.printf("[");
            for i in (sizeof(arg) / sizeof(unsigned long) - 1) {
                clib.printf("%lu, ", arg[i]);
            }
            clib.printf("%lu]", arg[sizeof(arg) / sizeof(unsigned long) - 1]);
"""

start = code.find('} else if typeof(arg) == "long" {')
end = code.find('          else {', start)

if start != -1 and end != -1:
    code = code[:start] + new_cases + code[end:]

with open('lib/std/print.aky', 'w') as f:
    f.write(code)
