# Analysis Report: Canonical `VarType*` Pointer System (Milestone 1)

**Working Directory**: `/home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_m1_2`  
**Milestone**: M1 (String Interning & Canonical Type Pointers)  
**Author**: Explorer 2  
**Date**: 2026-08-08  

---

## 1. Executive Summary

This report provides a comprehensive architectural analysis and implementation blueprint for refactoring Alkyl's type representation system from pass-by-value `VarType` structures to a canonical `VarType*` pointer system with hash-consing (interning). 

Under the canonical `VarType*` design:
- Type equality `types_are_equal(a, b)` and `sem_types_are_equal(a, b)` resolves to a single $O(1)$ pointer comparison (`a == b`).
- Structural string lookups during type comparison are completely eliminated.
- Memory footprint is reduced by sharing immutable type singletons across AST nodes, symbol tables, ALIR instructions, and codegen.

---

## 2. Core Interface & Interning Pool Design

### 2.1 Interface Definition (`include/common/types.h`)

`include/common/types.h` will define the canonical type structure and public API:

```c
#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdbool.h>
#include <stddef.h>

typedef enum VarTypeKind {
  TYPE_VOID,
  TYPE_INT,
  TYPE_UNSIGNED_INT,
  TYPE_SHORT,
  TYPE_LONG,
  TYPE_LONG_LONG,
  TYPE_UNSIGNED_LONG,
  TYPE_UNSIGNED_LONG_LONG,
  TYPE_CHAR,
  TYPE_UNSIGNED_CHAR,
  TYPE_BOOL,
  TYPE_SINGLE,
  TYPE_DOUBLE,
  TYPE_LONG_DOUBLE,
  TYPE_ARRAY,
  TYPE_AUTO,
  TYPE_CLASS,
  TYPE_ENUM,
  TYPE_NAMESPACE,
  TYPE_ERROR,
  TYPE_UNKNOWN
} VarTypeKind;

typedef VarTypeKind BaseType;

typedef struct VarType {
  VarTypeKind base;
  int ptr_depth;
  const char *class_name; // Interned string via intern_string()
  int array_size;
  int array_depth;

  const struct VarType *fp_ret_type;
  const struct VarType **fp_param_types;
  int fp_param_count;

  bool is_unsigned : 1;
  bool is_func_ptr : 1;
  bool fp_is_varargs : 1;
  bool is_tainted : 1;
  bool is_pristine : 1;
} VarType;

// Canonical Type Factory (Hash-Consing Interneer)
VarType* get_canonical_type(VarTypeKind kind, int ptr_depth, const char *class_name,
                            int array_size, int array_depth,
                            const VarType *fp_ret_type, const VarType **fp_param_types,
                            int fp_param_count, bool is_unsigned, bool is_func_ptr,
                            bool fp_is_varargs, bool is_tainted, bool is_pristine);

// Helper convenience constructors
VarType* get_primitive_type(VarTypeKind kind);
VarType* get_pointer_type(const VarType *base_type);
VarType* get_array_type(const VarType *element_type, int size);
VarType* get_class_type(const char *class_name, int ptr_depth);

// Pointer equality comparison: O(1)
static inline bool types_are_equal(const VarType* a, const VarType* b) {
    return a == b;
}

// Global pre-allocated primitive singletons
extern VarType *g_type_void;
extern VarType *g_type_int;
extern VarType *g_type_uint;
extern VarType *g_type_short;
extern VarType *g_type_long;
extern VarType *g_type_long_long;
extern VarType *g_type_ulong;
extern VarType *g_type_ulong_long;
extern VarType *g_type_char;
extern VarType *g_type_uchar;
extern VarType *g_type_bool;
extern VarType *g_type_single;
extern VarType *g_type_double;
extern VarType *g_type_long_double;
extern VarType *g_type_auto;
extern VarType *g_type_error;
extern VarType *g_type_unknown;

void init_types_system(void);

#endif // COMMON_TYPES_H
```

### 2.2 Interning Pool Implementation (`src/common/types.c`)

`src/common/types.c` will maintain a hash table of allocated `VarType` structures. 
1. Any incoming `class_name` string is canonicalized via `intern_string()`.
2. A 64-bit hash (FNV-1a / MurmurHash) is computed over all structural fields.
3. On collision or match, exact field verification is performed. If a match exists, the existing `VarType*` pointer is returned.
4. If absent, a new `VarType` struct is allocated, stored in the interning bucket, and returned.

---

## 3. Codebase Structural Mapping & Modifications

### 3.1 Parser AST Structs (`include/parser/typestruct.h`)

All `VarType` value fields in AST node definitions must be refactored to `VarType*` pointers:

| Struct | Existing Field | Refactored Field |
|---|---|---|
| `ASTNode` | `VarType sem_type;` | `VarType *sem_type;` |
| `VarDeclNode` | `VarType var_type;` | `VarType *var_type;` |
| `FuncDefNode` | `VarType ret_type;` | `VarType *ret_type;` |
| `Parameter` | `VarType type;` | `VarType *type;` |
| `CastNode` | `VarType var_type;` | `VarType *var_type;` |
| `BeingNode` | `VarType var_type;` | `VarType *var_type;` |
| `ForInNode` | `VarType iter_type;` | `VarType *iter_type;` |
| `LiteralNode` | `VarType var_type;` | `VarType *var_type;` |
| `SizeOfNode` | `VarType target_type;` | `VarType *target_type;` |
| `IsCompatibleNode` | `VarType target_type, target_type2;` | `VarType *target_type, *target_type2;` |
| `TemplateInstNode` | `VarType *template_types;` | `VarType **template_types;` |
| `CompoundNode` | `VarType **allowed_types;` | `VarType ***allowed_types;` |

### 3.2 Parser Logic (`src/parser/`)

- `src/parser/core.c`: Refactor `parse_type(Parser *p)` to build temporary attributes and call `get_canonical_type(...)`, returning a canonical `VarType*`.
- `src/parser/ast_clone.c`: Refactor `clone_var_type` to map old `VarType*` to replaced `VarType*` pointers via `get_canonical_type`. Because `VarType` pointers are immutable and shared, cloning an AST node only requires copying the `VarType*` pointer address.

### 3.3 Semantic Analyzer (`src/semantic/`, `include/semantic/`)

1. **Symbol Table & Scope (`include/semantic/typestruct.h`)**:
   - `SemSymbol`: `VarType type;` -> `VarType *type;`
   - `SemScope`: `VarType expected_ret_type;` -> `VarType *expected_ret_type;`
   - `TypeEntry`: `VarType type;` -> `VarType *type;`

2. **Type Equality Refactoring (`src/semantic/table.c:470`)**:
   `sem_types_are_equal(VarType a, VarType b)` currently performs a multi-field check with string mangling and comparison.
   **Refactored**:
   ```c
   int sem_types_are_equal(const VarType *a, const VarType *b) {
       return a == b;
   }
   ```

3. **Union Synthetic Types (`__Union_...`) & Two-Pass Matching**:
   In `src/semantic/type.c` (lines 428, 436), `src/semantic/check.c` (lines 519, 537), and `src/semantic/fragment/switch.c` (lines 223, 241):
   - **Pass 1 (Exact Match)**: `f->kind == SYM_VAR && (f->type == target_type)` (or `sem_types_are_equal(f->type, target_type)`).
   - **Pass 2 (Compatible Match Fallback)**: `f->kind == SYM_VAR && sem_types_are_compatible(ctx, f->type, target_type)`.
   With canonical pointers, Pass 1 is an instant $O(1)$ pointer comparison `f->type == target_type`.

### 3.4 ALIR & Codegen (`include/alir/`, `src/alir/`, `src/codegen_llvm/`, `src/codegen_qbe/`)

- `AlirValue`, `AlirFunction`, `AlirParam`, `AlirGlobal` store `VarType *type`.
- ALIR constructors (`new_temp`, `promote`, `alir_add_symbol`, `alir_val_temp`, `alir_val_global`, `alir_add_function`, `alir_func_add_param`) accept `VarType *type`.
- ALIR binary serialization (`binary_write.c` and `binary_read.c`) serializes type metadata and re-constitutes canonical `VarType*` via `get_canonical_type`.
- Codegen switch statements dereference `t->base` and `t->ptr_depth`.

---

## 4. User Rules Compliance Audit

1. **String Comparison Rule**:
   - `strcmp` will NOT be used for type or string comparisons.
   - All string comparison usages will rely on `streq(a, b)` or pointer equality `a == b` (enabled by interned strings).
2. **Debug Print Rule**:
   - No direct `printf` or `fprintf(stderr, "DEBUG:...")` calls allowed for debugging.
   - Module-specific macros (`debug_parser`, `debug_semantic`, `debug_alir`, `debug_lexer`, `debug_codegen`) or `debug_any` must be used.

---

## 5. Implementation Roadmap for M1 Implementer

1. **Step 1**: Create `include/common/types.h` and `src/common/types.c`. Implement `get_canonical_type` interning table and global primitive singletons (`g_type_int`, etc.).
2. **Step 2**: Update `include/parser/typestruct.h` to change all node type fields to `VarType*`.
3. **Step 3**: Update `src/parser/` (`core.c`, `ast_clone.c`, `stmt.c`, `expr.c`, `top.c`) to build canonical `VarType*` pointers.
4. **Step 4**: Update `include/semantic/typestruct.h` and `src/semantic/` (`table.c`, `type.c`, `check.c`, `symbol.c`, `fragment/`, `modifier/`). Replace `sem_types_are_equal` body with `return a == b;`.
5. **Step 5**: Update `include/alir/` and `src/alir/` to store `VarType*`.
6. **Step 6**: Update `src/codegen_llvm/` and `src/codegen_qbe/` type translation functions.
7. **Step 7**: Build with `make` and run opaque test suite (`scripts/run_tests.sh`).
