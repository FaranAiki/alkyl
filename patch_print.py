import re

with open('lib/std/print.kyl', 'r') as f:
    code = f.read()

# Replace c_lib with alkyl runtime functions
old_c_lib = '''namespace __c_lib {
  extern int printf(char*, ...);
  extern int putchar(int);
  extern int puts(char*);
}

define formatted_printf as __c_lib.printf;'''

new_c_lib = '''namespace __c_lib {
  extern int putchar(int);
  extern int puts(char*);
  extern void alkyl_print_str(char*);
  extern void alkyl_print_char(char);
  extern void alkyl_print_i64(long);
  extern void alkyl_print_u64(unsigned long);
  extern void alkyl_print_i32(int);
  extern void alkyl_print_u32(unsigned int);
  extern void alkyl_print_f32(single);
  extern void alkyl_print_f64(double);
}'''

code = code.replace(old_c_lib, new_c_lib)

# Replace print_int_array etc
for ty, wrap_name in [('int', 'alkyl_print_i32'), ('uint', 'alkyl_print_u32'), ('long', 'alkyl_print_i64'), ('ulong', 'alkyl_print_u64'), ('single', 'alkyl_print_f32'), ('double', 'alkyl_print_f64')]:
    code = re.sub(
        r'void print_' + ty + r'_array\([^)]+\)\s*\{[^{]*?formatted_printf\("[^"]+",([^)]+)\);.*?formatted_printf\("%s", ", "\);.*?\}',
        lambda m: m.group(0).replace('formatted_printf("%s", ", ")', '__c_lib.alkyl_print_str(", ")'),
        code,
        flags=re.DOTALL
    )

code = re.sub(r'formatted_printf\("%d", (.*?)\);', r'__c_lib.alkyl_print_i32(\1);', code)
code = re.sub(r'formatted_printf\("%u", (.*?)\);', r'__c_lib.alkyl_print_u32(\1);', code)
code = re.sub(r'formatted_printf\("%ld", (.*?)\);', r'__c_lib.alkyl_print_i64(\1);', code)
code = re.sub(r'formatted_printf\("%lu", (.*?)\);', r'__c_lib.alkyl_print_u64(\1);', code)
code = re.sub(r'formatted_printf\("%f", (.*?)\);', r'__c_lib.alkyl_print_f32(\1);', code)
code = re.sub(r'formatted_printf\("%lf", (.*?)\);', r'__c_lib.alkyl_print_f64(\1);', code)
code = re.sub(r'formatted_printf\("%c", (.*?)\);', r'__c_lib.alkyl_print_char(\1);', code)
code = re.sub(r'formatted_printf\("%s", (.*?)\);', r'__c_lib.alkyl_print_str(\1);', code)

with open('lib/std/print.kyl', 'w') as f:
    f.write(code)
