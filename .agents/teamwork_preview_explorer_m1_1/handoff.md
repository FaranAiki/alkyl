# Handoff Report: M1 Global String Interning & Canonical String Pointers

**Agent:** Explorer 1  
**Milestone:** M1 (String Interning & Canonical Type Pointers)  
**Working Directory:** `/home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_m1_1`  
**Date:** 2026-08-08  

---

## 1. Observation

1. **Missing Files for Global Interner:**
   A search using `find_by_name` in `include/` and `src/` for `*intern*` showed that `include/common/intern.h` and `src/common/intern.c` do not exist in the project repository.
2. **Current Lexer String Allocation:**
   In `src/lexer/lexer.c` lines 41–67:
   ```c
   static char* intern_string(Lexer *l, const char *str) {
       ...
       void *existing = hashmap_get(&l->ctx->string_pool, str);
       ...
   }
   static char* intern_strndup(Lexer *l, const char *str, size_t len) {
       void *existing = hashmap_get_n(&l->ctx->string_pool, str, len);
       ...
   }
   ```
   Lexer strings are stored in `CompilerContext.string_pool` (per-context hashmap).
3. **Current Parser String Allocation:**
   In `src/parser/core.c` lines 71–74:
   ```c
   char* parser_strdup(Parser *p, const char *str) {
       if (!p || !p->ctx || !p->ctx->arena) return strdup(str);
       return arena_strdup(p->ctx->arena, str);
   }
   ```
   `parser_strdup` allocates string buffers inside the AST parser arena via `arena_strdup`.
4. **Current Semantic Symbol & String Allocation:**
   In `src/semantic/table.c` lines 208–211:
   ```c
   SemSymbol *sym = arena_alloc_type(ctx->compiler_ctx->arena, SemSymbol);
   sym->name = arena_strdup(ctx->compiler_ctx->arena, name);
   ```
   Symbol names and type `class_name` fields are allocated via `arena_strdup`.
5. **Existing `streq` Implementation:**
   In `include/common/common.h` lines 42–47:
   ```c
   static inline int streq(const char *a, const char *b) {
       if (a == b) return 1;
       if (!a || !b) return 0;
       if (a[0] != b[0]) return 0;
       return strcmp(a, b) == 0;
   }
   ```
6. **Direct `strcmp` Usages:**
   Grep search for `strcmp` across `src/` revealed direct `strcmp` calls in `src/semantic/table.c` (lines 287, 299, 309, 376, 442) and `src/mlir/expr.c` (lines 35, 50, 89, 197, 201, 223, 229, 255, 287).

---

## 2. Logic Chain

1. **Observation 1 & 2:** Lexer strings are currently interned per `CompilerContext` rather than globally, preventing canonical pointer sharing across contexts, REPL sessions, or multi-threaded stages.
2. **Observation 3 & 4:** Parser AST nodes and Semantic symbol objects allocate independent string copies using `arena_strdup`, causing duplicate memory addresses for identical string names (e.g. `"int"`, `"main"`, variable identifiers).
3. **Observation 5:** `streq(a, b)` checks `if (a == b) return 1;` before any string scanning. If all string identifiers are interned through a global interner (`intern_string`), `a == b` will be true for identical strings, making string comparisons $O(1)$.
4. **Observation 6:** Replacing direct `strcmp` calls in `src/semantic/table.c` and `src/mlir/expr.c` with `streq` or direct pointer equality `a == b` fulfills the user rule prohibiting `strcmp` in the compiler codebase.

---

## 3. Caveats

1. `streq(a, b)` retains fallback `strcmp` execution for cases where non-interned strings (e.g. dynamic user input strings or literal strings during transition) are passed. This ensures safe behavior during refactoring.
2. `intern_string_len` handles non-null-terminated string slices by duplicating `len` bytes into the global interner arena before inserting into the global hash map.
3. Thread safety: If parallel ALIR (M4) or concurrent compilation is enabled in later milestones, the global interner hashtable will require atomic or mutex protection if accessed concurrently.

---

## 4. Conclusion

Implementation of `include/common/intern.h` and `src/common/intern.c` with `intern_string` and `intern_string_len` provides the canonical foundation for M1. 
Refactoring `lexer.c`, `parser/core.c`, `parser/ast_clone.c`, `semantic/table.c`, and `context.c` to delegate string allocations to `intern_string` will guarantee canonical interned string pointers across all compiler stages and make `streq` / `a == b` string comparisons $O(1)$.

All details and step-by-step implementation code snippets are documented in `/home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_m1_1/analysis.md`.

---

## 5. Verification Method

1. **Build Verification:**
   Run `make` to compile the `alkyl` executable and `ethyl` REPL.
2. **Test Suite Verification:**
   Run `scripts/run_tests.sh` or `./test.sh` to verify zero test regressions.
3. **Pointer Equality Verification:**
   Verify `assert(intern_string("my_var") == intern_string("my_var"))` returns true (identical pointers).
4. **`strcmp` Elimination Check:**
   Run:
   ```bash
   grep -rn "strcmp" src/ include/ | grep -v "include/common/common.h"
   ```
   Confirm output is empty (0 remaining `strcmp` calls in compiler source code).
