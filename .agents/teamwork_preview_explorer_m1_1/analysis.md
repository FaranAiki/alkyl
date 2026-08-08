# Analysis Report: Global String Interning & Canonical String Pointers (M1)

**Author:** Explorer 1  
**Milestone:** M1 (String Interning & Canonical Type Pointers)  
**Working Directory:** `/home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_m1_1`  
**Date:** 2026-08-08  

---

## 1. Executive Summary

This investigation analyzed the Alkyl compiler codebase to design and plan the implementation of a **Global String Interner** (`include/common/intern.h` and `src/common/intern.c`). 

Currently, string duplication and interning are handled per-context via `CompilerContext.string_pool` or per-arena using `arena_strdup`. Furthermore, direct `strcmp` calls exist in parts of the semantic analyzer and backend (`src/semantic/table.c`, `src/mlir/expr.c`).

By establishing a centralized, process-wide Global String Interner:
1. All token text, symbol names, function/class/mangled identifiers, variable names, and string literals will share canonical, deduplicated `const char*` pointers.
2. String comparison via `streq(a, b)` (defined in `include/common/common.h`) or direct pointer equality `a == b` will run in **$O(1)$ time** because identical string contents are guaranteed to share identical memory addresses.
3. All legacy `strcmp` calls in the codebase can be replaced with `streq` or direct `a == b` pointer comparisons.

---

## 2. Current Codebase State & Architectural Audit

### 2.1 Missing Files
The global interner interface headers and implementation files specified in `PROJECT.md` do not exist yet:
- `include/common/intern.h` (Missing)
- `src/common/intern.c` (Missing)

### 2.2 Current String Allocation & Storage Across Modules

#### A. `src/lexer/` (`src/lexer/lexer.c`)
- **Current Behavior:** Identifiers and string literal contents use local static functions `intern_string(l, str)` and `intern_strndup(l, str, len)` (lines 41–67 in `src/lexer/lexer.c`). These query `l->ctx->string_pool`, which is a per-`CompilerContext` hashmap.
- **Problem:** Strings interned in one context are not shared globally across different contexts, REPL sessions, or multi-threaded stages.

#### B. `src/parser/` (`src/parser/core.c`, `src/parser/ast_clone.c`, `include/parser/typestruct.h`)
- **Current Behavior:** AST string fields (`name`, `mangled_name`, `class_name`, `cconv`, `extern_name`, `parent_name`, `member_name`, `method_name`, `var_name`, `err_var_name`, `path`) are allocated using `parser_strdup(p, str)` (line 71 in `src/parser/core.c`) or `arena_strdup(ctx->arena, str)` (in `ast_clone.c`).
- **`parser_strdup` Implementation:** Currently delegates to `arena_strdup(p->ctx->arena, str)`.

#### C. `src/semantic/` (`src/semantic/table.c`, `src/semantic/check.c`, `src/semantic/type.c`, `src/semantic/fragment/*`)
- **Current Behavior:** Symbol creation (`sem_symbol_add` in `src/semantic/table.c:211`) allocates `sym->name` via `arena_strdup(ctx->compiler_ctx->arena, name)`. Similar `arena_strdup` calls populate `VarType.class_name`, `sym->parent_name`, `sym->traits`, `sym->mangled_name`, and `node->overloaded_func_name`.
- **Lookups:** `find_in_scope_direct` (lines 111–131 in `table.c`) looks up symbols in `SemScope.symbol_map` (`HashMap`) using `hashmap_get` or linear scan using `streq(sym->name, name)`.

#### D. `src/common/context.c`
- **Current Behavior:** `context_intern(ctx, str)` (lines 46–49) calls `hashmap_intern(&ctx->string_pool, str)`.

### 2.3 Audit of String Comparisons & `strcmp` Usages

1. **`include/common/common.h` (Line 42):**
   ```c
   static inline int streq(const char *a, const char *b) {
       if (a == b) return 1;
       if (!a || !b) return 0;
       if (a[0] != b[0]) return 0;
       return strcmp(a, b) == 0;
   }
   ```
   *Observation:* `streq` already checks `if (a == b) return 1;` as its first operation. When string interning is complete, identical strings will have `a == b`, making `streq` execute in $O(1)$ time without falling through to character scanning or `strcmp`.

2. **Direct `strcmp` Violations Found in Compiler Codebase:**
   - `src/semantic/table.c` (lines 287, 299, 309, 376, 442):
     ```c
     if (!(sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0))
     ```
   - `src/mlir/expr.c` (lines 35, 50, 89, 197, 201, 223, 229, 255, 287):
     ```c
     if (strcmp(ent->name, var->name) == 0) ...
     ```
   *Action:* Replace all these direct `strcmp` instances with `streq(a, b)` or `a == b`.

---

## 3. Proposed Implementation Architecture

### 3.1 Global String Interner Interface (`include/common/intern.h`)

```c
#ifndef COMMON_INTERN_H
#define COMMON_INTERN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Global String Interner Interface
// Returns a canonical, immutable, null-terminated interned string pointer.
// Guarantee: If string contents of str1 and str2 are equal, intern_string(str1) == intern_string(str2).
const char* intern_string(const char* str);
const char* intern_string_len(const char* str, size_t len);

#ifdef __cplusplus
}
#endif

#endif // COMMON_INTERN_H
```

### 3.2 Global String Interner Implementation (`src/common/intern.c`)

```c
#include "common/intern.h"
#include "common/hashmap.h"
#include "common/arena.h"
#include "common/common.h"
#include <string.h>

static HashMap g_intern_map;
static Arena g_intern_arena;
static bool g_intern_inited = false;

static void global_intern_init(void) {
    if (!g_intern_inited) {
        arena_init(&g_intern_arena);
        hashmap_init(&g_intern_map, &g_intern_arena, 4096);
        g_intern_inited = true;
    }
}

const char* intern_string_len(const char* str, size_t len) {
    if (!str) return NULL;
    global_intern_init();

    // Check if string slice is already interned
    void *existing = hashmap_get_n(&g_intern_map, str, len);
    if (existing) {
        return (const char*)existing;
    }

    // Allocate canonical copy in global intern arena
    char *copy = arena_strndup(&g_intern_arena, str, len);
    hashmap_put(&g_intern_map, copy, (void*)copy);
    return copy;
}

const char* intern_string(const char* str) {
    if (!str) return NULL;
    return intern_string_len(str, strlen(str));
}
```

### 3.3 Integration Plan Across Compiler Modules

1. **`src/lexer/lexer.c`**:
   - Update `intern_string(Lexer *l, const char *str)` and `intern_strndup(Lexer *l, const char *str, size_t len)` to call global `intern_string(str)` and `intern_string_len(str, len)`.

2. **`src/parser/core.c` & `src/parser/ast_clone.c`**:
   - Refactor `parser_strdup(Parser *p, const char *str)` to return `intern_string(str)`.
   - Refactor `ast_clone.c` string field duplication to use `intern_string(str)`.

3. **`src/semantic/table.c`, `check.c`, `type.c`**:
   - Update `sem_symbol_add` and symbol creation functions to set `sym->name = intern_string(name)`.
   - Update `VarType` class name assignments to set `var_type.class_name = intern_string(name)`.
   - Replace all `strcmp` calls in `table.c` with `streq` or `a == b`.

4. **`src/common/context.c`**:
   - Update `context_intern(CompilerContext *ctx, const char *str)` to delegate to `intern_string(str)`.

5. **`src/mlir/expr.c`**:
   - Replace all `strcmp` calls with `streq` or `a == b`.

---

## 4. Verification Plan

1. **Unit & Pointer Equality Test:**
   - Verify `intern_string("abc") == intern_string("abc")`.
   - Verify `intern_string_len("abc123", 3) == intern_string("abc")`.
2. **Regression Testing:**
   - Run `scripts/run_tests.sh` to ensure no existing compilation behavior is broken.
3. **No `strcmp` Violation Check:**
   - Grep search codebase for `strcmp` to confirm no direct `strcmp` remains outside `streq` in `include/common/common.h`.
4. **Debug Macro Compliance Check:**
   - Ensure no raw `printf`/`fprintf` calls exist for debugging outside module-specific `debug_*` macros.
