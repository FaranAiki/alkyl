#!/usr/bin/env python3
"""
Modularize parser files >500 lines into smaller units.
"""
import os
import re

BASE = '/home/faranaiki/Git/alkyl'

def ensure_dir(path):
    os.makedirs(path, exist_ok=True)

def write_file(path, content):
    with open(path, 'w') as f:
        f.write(content)

def extract_functions(content):
    """Extract all function definitions from C source content."""
    functions = []
    i = 0
    while i < len(content):
        # Skip preprocessor directives
        if content[i] == '#':
            while i < len(content) and content[i] != '\n':
                i += 1
            continue
        # Skip single-line comments
        if content[i] == '/' and i+1 < len(content) and content[i+1] == '/':
            while i < len(content) and content[i] != '\n':
                i += 1
            continue
        # Skip multi-line comments
        if content[i] == '/' and i+1 < len(content) and content[i+1] == '*':
            i += 2
            while i < len(content) and not (content[i] == '*' and i+1 < len(content) and content[i+1] == '/'):
                i += 1
            i += 2
            continue
        
        # Try to match function definition
        match = re.match(r'^(static\s+)?([a-zA-Z_][a-zA-Z0-9_\s\*]+?)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(', content[i:])
        if match:
            is_static = bool(match.group(1))
            return_type = match.group(2).strip()
            func_name = match.group(3)
            func_start = i
            
            j = i + match.end()
            while j < len(content) and content[j] != '{' and content[j] != ';':
                j += 1
            
            if j < len(content) and content[j] == '{':
                brace_count = 1
                in_string = False
                string_char = None
                escape = False
                j += 1
                while j < len(content) and brace_count > 0:
                    ch = content[j]
                    if escape:
                        escape = False
                    elif ch == '\\':
                        escape = True
                    elif in_string:
                        if ch == string_char:
                            in_string = False
                    elif ch == '"':
                        in_string = True
                        string_char = ch
                    elif ch == '{':
                        brace_count += 1
                    elif ch == '}':
                        brace_count -= 1
                    j += 1
                func_body = content[func_start:j]
                functions.append({
                    'name': func_name,
                    'static': is_static,
                    'return': return_type,
                    'body': func_body,
                    'start': func_start,
                    'end': j
                })
                i = j
                continue
            else:
                i = j + 1
                continue
        i += 1
    return functions

def create_group_files(groups, src_dir, hdr_dir, base_include, extra_decls=None):
    """Write .c and .h files for each group."""
    ensure_dir(src_dir)
    ensure_dir(hdr_dir)
    
    all_decls = []
    for group_name, func_names in groups.items():
        bodies = []
        for func in func_names:
            bodies.append(func['body'])
            # Extract actual signature from function body
            m = re.match(r'^(static\s+)?([a-zA-Z_][a-zA-Z0-9_\s\*]+?)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)\)', func['body'])
            if m:
                return_type = m.group(2).strip()
                func_name = m.group(3)
                params = m.group(4).strip()
                # Only include declaration if it takes Parser *p as first param
                if params.startswith('Parser') or not params:
                    all_decls.append(f"{return_type} {func_name}({params});")
        
        if bodies:
            code = f'#include "{base_include}"\n#include <string.h>\n#include <stdlib.h>\n\n' + '\n\n'.join(bodies)
            write_file(f'{src_dir}/{group_name}.c', code)
    
    if extra_decls:
        all_decls.extend(extra_decls)
    
    guard_name = os.path.basename(hdr_dir).upper()
    write_file(f'{hdr_dir}/{os.path.basename(hdr_dir)}.h',
               f'#ifndef PARSER_{guard_name}_H\n#define PARSER_{guard_name}_H\n\n'
               '#include "parser.h"\n\n' + '\n'.join(all_decls) + '\n\n#endif\n')

# ============================================================
# 1. SPLIT expr.c (970 lines)
# ============================================================
print("Splitting expr.c...")
with open(f'{BASE}/src/parser/expr.c') as f:
    expr_content = f.read()
expr_funcs = extract_functions(expr_content)
expr_map = {f['name']: f for f in expr_funcs}

expr_groups = [
    ('expr_call', [expr_map['parse_call'], expr_map['is_unambiguous_expr_start'], expr_map['parse_space_separated_call']]),
    ('expr_postfix', [expr_map['parse_postfix']]),
    ('expr_factor', [expr_map['parse_factor']]),
    ('expr_unary', [expr_map['parse_unary']]),
    ('expr_op', [expr_map['parse_binary_op'], expr_map['parse_term'], expr_map['parse_additive'], 
                 expr_map['parse_shift'], expr_map['parse_relational'], expr_map['parse_equality'],
                 expr_map['parse_bitwise_and'], expr_map['parse_bitwise_xor'], expr_map['parse_bitwise_or'],
                 expr_map['parse_logic_and'], expr_map['parse_logic_or']]),
    ('expr_high', [expr_map['parse_fallback'], expr_map['parse_assignment'], 
                   expr_map['parse_expression'], expr_map['parse_initializer']]),
]

create_group_files({g[0]: g[1] for g in expr_groups},
                   f'{BASE}/src/parser/expr',
                   f'{BASE}/include/parser/expr',
                   'parser_internal.h')

os.remove(f'{BASE}/src/parser/expr.c')
print("  expr.c done")

# ============================================================
# 2. SPLIT core.c (845 lines)
# ============================================================
print("Splitting core.c...")
with open(f'{BASE}/src/parser/core.c') as f:
    core_content = f.read()
core_funcs = extract_functions(core_content)
core_map = {f['name']: f for f in core_funcs}

core_groups = [
    ('parser_init', [core_map['parser_init'], core_map['parser_alloc'], core_map['parser_strdup']]),
    ('parser_error', [core_map['parser_fail_at'], core_map['parser_fail'], core_map['parser_sync'], core_map['eat']]),
    ('parser_type', [core_map['register_typename'], core_map['is_typename'], core_map['is_type_start'],
                     core_map['get_typename_kind'], core_map['register_alias'], core_map['get_alias']]),
    ('parser_parse_type', [core_map['parse_type'], core_map['parse_func_ptr_decl']]),
    ('parser_macro', [core_map['register_macro'], core_map['find_macro'], core_map['macro_is_expanding'],
                      core_map['expand_macros_from'], core_map['lexer_next_raw'], core_map['get_next_token_expanded'],
                      core_map['fetch_safe']]),
    ('parser_import', [core_map['token_clone'], core_map['read_import_file'], core_map['parser_prescan']]),
]

create_group_files({g[0]: g[1] for g in core_groups},
                   f'{BASE}/src/parser/core',
                   f'{BASE}/include/parser/core',
                   'parser_internal.h')

os.remove(f'{BASE}/src/parser/core.c')
print("  core.c done")

# ============================================================
# 3. SPLIT top.c (813 lines)
# ============================================================
print("Splitting top.c...")
with open(f'{BASE}/src/parser/top.c') as f:
    top_content = f.read()
top_funcs = extract_functions(top_content)
top_map = {f['name']: f for f in top_funcs}

top_groups = [
    ('top_stmt', [top_map['apply_implicit_return']]),
    ('top_extern', [top_map['parse_single_extern'], top_map['parse_extern']]),
    ('top_func', [top_map['parse_func_def_after_type']]),
    ('top_errnum', [top_map['parse_errnum']]),
    ('top_program', [top_map['parse_compound'], top_map['parse_top_level'], top_map['parse_top_level_internal']]),
]

create_group_files({g[0]: g[1] for g in top_groups},
                   f'{BASE}/src/parser/top',
                   f'{BASE}/include/parser/top',
                   'parser_internal.h')

os.remove(f'{BASE}/src/parser/top.c')
print("  top.c done")

# ============================================================
# 4. SPLIT emitter.c (636 lines)
# ============================================================
print("Splitting emitter.c...")
with open(f'{BASE}/src/parser/emitter.c') as f:
    emitter_content = f.read()
emitter_funcs = extract_functions(emitter_content)
emitter_map = {f['name']: f for f in emitter_funcs}

emitter_groups = [
    ('emit_expr', [emitter_map['parser_emit_ast_node']]),
    ('emit_stmt', [emitter_map['parser_emit_block']]),
    ('emit_type', [emitter_map['parser_emit_type'], emitter_map['needs_semicolon']]),
    ('emit_toplevel', [emitter_map['parser_to_string'], emitter_map['parser_to_file'],
                       emitter_map['parser_string_to_string'], emitter_map['parser_string_to_file']]),
    ('emit_helpers', [emitter_map['parser_emit_indent']]),
]

create_group_files({g[0]: g[1] for g in emitter_groups},
                   f'{BASE}/src/parser/emitter',
                   f'{BASE}/include/parser/emitter',
                   'parser_internal.h')

os.remove(f'{BASE}/src/parser/emitter.c')
print("  emitter.c done")

# ============================================================
# 5. SPLIT stmt.c (556 lines)
# ============================================================
print("Splitting stmt.c...")
with open(f'{BASE}/src/parser/stmt.c') as f:
    stmt_content = f.read()
stmt_funcs = extract_functions(stmt_content)
stmt_map = {f['name']: f for f in stmt_funcs}

stmt_groups = [
    ('parse_stmt', [stmt_map['parse_single_statement_or_block'], stmt_map['parse_residue_cases']]),
    ('parse_control', [stmt_map['parse_return'], stmt_map['parse_emit'], stmt_map['parse_assignment_or_call']]),
]

create_group_files({g[0]: g[1] for g in stmt_groups},
                   f'{BASE}/src/parser/stmt',
                   f'{BASE}/include/parser/stmt',
                   'parser_internal.h',
                   ['void set_loc(ASTNode *n, int line, int col);'])

os.remove(f'{BASE}/src/parser/stmt.c')
print("  stmt.c done")

# ============================================================
# 6. SPLIT ast_clone.c (567 lines)
# ============================================================
print("Splitting ast_clone.c...")
with open(f'{BASE}/src/parser/ast_clone.c') as f:
    ast_clone_content = f.read()
ast_clone_funcs = extract_functions(ast_clone_content)
ast_clone_map = {f['name']: f for f in ast_clone_funcs}

ast_clone_groups = [
    ('clone_util', [ast_clone_map['clone_var_type']]),
    ('clone_main', [ast_clone_map['ast_clone']]),
    ('clone_macro', [ast_clone_map['ast_rewrite_macro']]),
]

create_group_files({g[0]: g[1] for g in ast_clone_groups},
                   f'{BASE}/src/parser/ast_clone',
                   f'{BASE}/include/parser/ast_clone',
                   'parser_internal.h')

os.remove(f'{BASE}/src/parser/ast_clone.c')
print("  ast_clone.c done")

# ============================================================
# 7. SPLIT fragment/class.c (477 lines)
# ============================================================
print("Splitting fragment/class.c...")
with open(f'{BASE}/src/parser/fragment/class.c') as f:
    class_content = f.read()
class_funcs = extract_functions(class_content)
class_map = {f['name']: f for f in class_funcs}

class_groups = [
    ('class', [class_map['parse_enum'], class_map['parse_class_impl'], class_map['parse_class']]),
]

create_group_files({g[0]: g[1] for g in class_groups},
                   f'{BASE}/src/parser/fragment/class',
                   f'{BASE}/include/parser/fragment/class',
                   'parser_internal.h')

os.remove(f'{BASE}/src/parser/fragment/class.c')
print("  fragment/class.c done")

# ============================================================
# Update CMakeLists.txt
# ============================================================
print("Updating CMakeLists.txt...")
with open(f'{BASE}/CMakeLists.txt') as f:
    cmake = f.read()

# Remove old source entries
old_entries = [
    '    src/parser/core.c',
    '    src/parser/expr.c',
    '    src/parser/stmt.c',
    '    src/parser/top.c',
    '    src/parser/emitter.c',
    '    src/parser/ast_clone.c',
    '    src/parser/fragment/class.c',
]
for old in old_entries:
    cmake = cmake.replace(old + '\n', '')

# Add new sources after src/parser/link.c
new_sources = '''    src/parser/core/parser_init.c
    src/parser/core/parser_error.c
    src/parser/core/parser_type.c
    src/parser/core/parser_parse_type.c
    src/parser/core/parser_macro.c
    src/parser/core/parser_import.c
    src/parser/expr/expr_call.c
    src/parser/expr/expr_postfix.c
    src/parser/expr/expr_factor.c
    src/parser/expr/expr_unary.c
    src/parser/expr/expr_op.c
    src/parser/expr/expr_high.c
    src/parser/stmt/parse_stmt.c
    src/parser/stmt/parse_control.c
    src/parser/stmt/parse_decl.c
    src/parser/top/top_stmt.c
    src/parser/top/top_extern.c
    src/parser/top/top_func.c
    src/parser/top/top_errnum.c
    src/parser/top/top_program.c
    src/parser/emitter/emit_expr.c
    src/parser/emitter/emit_stmt.c
    src/parser/emitter/emit_type.c
    src/parser/emitter/emit_toplevel.c
    src/parser/emitter/emit_helpers.c
    src/parser/ast_clone/clone_util.c
    src/parser/ast_clone/clone_main.c
    src/parser/ast_clone/clone_macro.c
    src/parser/fragment/class/class.c
'''

# Find insertion point - after src/parser/link.c
insert_marker = '    src/parser/link.c\n'
if insert_marker in cmake:
    pos = cmake.index(insert_marker) + len(insert_marker)
    cmake = cmake[:pos] + new_sources + cmake[pos:]

with open(f'{BASE}/CMakeLists.txt', 'w') as f:
    f.write(cmake)

print("  CMakeLists.txt updated")

# ============================================================
# Update parser_internal.h
# ============================================================
print("Updating parser_internal.h...")
with open(f'{BASE}/include/parser/parser_internal.h') as f:
    internal_h = f.read()

new_includes = '''#include "parser/core/core.h"
#include "parser/top/top.h"
#include "parser/emitter/emitter.h"
#include "parser/stmt/stmt.h"
#include "parser/ast_clone/ast_clone.h"
#include "parser/fragment/class/class.h"
'''

# Replace old includes with new modular includes
old_includes = '''// Expressions (parser/expr.c)
#include "parser/expr/expr.h"

#include "modif.h"
#include "stmt.h"
#include "top.h"
#include "semantic.h"


void eat_semi(Parser *p);
void set_loc(ASTNode *n, int line, int col);

#include "parser/fragment/class.h"
#include "parser/fragment/cond.h"
#include "parser/fragment/loop.h"
#include "parser/fragment/decl.h"
'''

new_internal = '''// Expressions (parser/expr.c)
#include "parser/expr/expr.h"

#include "modif.h"
#include "stmt.h"
#include "top.h"
#include "semantic.h"


void eat_semi(Parser *p);
void set_loc(ASTNode *n, int line, int col);

#include "parser/fragment/class.h"
#include "parser/fragment/cond.h"
#include "parser/fragment/loop.h"
#include "parser/fragment/decl.h"

''' + new_includes

internal_h = internal_h.replace(old_includes, new_internal)

with open(f'{BASE}/include/parser/parser_internal.h', 'w') as f:
    f.write(internal_h)

print("  parser_internal.h updated")

print("\nAll modularization complete!")
