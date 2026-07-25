import re

with open('lib/std/print.aky', 'r') as f:
    code = f.read()

code = code.replace('typeof(arg) == "int"', 'typeof(arg) == typeof(int)')
code = code.replace('typeof(arg) == "char"', 'typeof(arg) == typeof(char)')
code = code.replace('typeof(arg) == "unsigned int"', 'typeof(arg) == typeof(unsigned int)')
code = code.replace('typeof(arg) == "long"', 'typeof(arg) == typeof(long)')
code = code.replace('typeof(arg) == "unsigned long"', 'typeof(arg) == typeof(unsigned long)')
code = code.replace('typeof(arg) == "long long"', 'typeof(arg) == typeof(long long)')
code = code.replace('typeof(arg) == "unsigned long long"', 'typeof(arg) == typeof(unsigned long long)')
code = code.replace('typeof(arg) == "single"', 'typeof(arg) == typeof(single)')
code = code.replace('typeof(arg) == "double"', 'typeof(arg) == typeof(double)')
code = code.replace('typeof(arg) == "int[]"', 'typeof(arg) == typeof(int[])')
code = code.replace('typeof(arg) == "unsigned int[]"', 'typeof(arg) == typeof(unsigned int[])')
code = code.replace('typeof(arg) == "long[]"', 'typeof(arg) == typeof(long[])')
code = code.replace('typeof(arg) == "unsigned long[]"', 'typeof(arg) == typeof(unsigned long[])')
code = code.replace('typeof(arg) == "char*"', 'typeof(arg) == typeof(char*)')

with open('lib/std/print.aky', 'w') as f:
    f.write(code)

