# Handoff Report: Explorer 2 (Milestone 1 — Canonical `VarType*` Pointer System)

**Working Directory**: `/home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_m1_2`  
**Milestone**: M1 (String Interning & Canonical Type Pointers)  
**Recipient**: Sub-Orchestrator / Implementer  
**Date**: 2026-08-08  

---

## 1. Observation

1. **Current Type Definition & Usage**:
   - `include/parser/typestruct.h` lines 96–112: `typedef struct VarType { ... } VarType;` is currently passed by value across AST nodes (`ASTNode.sem_type`, `VarDeclNode.var_type`, `FuncDefNode.ret_type`, `Parameter.type`, `LiteralNode.var_type`, `CastNode.var_type`, etc.).
   - `include/semantic/typestruct.h` lines 19, 80, 93: `SemSymbol.type`, `SemScope.expected_ret_type`, `TypeEntry.type` are stored as `VarType` value structs.
   - `include/alir/alir.h` lines 25, 106, 112, 133: `AlirValue.type`, `AlirFunction.ret_type`, `AlirParam.type`, `AlirGlobal.type` store `VarType` value structs.

2. **Existing Type Comparison Functions**:
   - `src/semantic/table.c` lines 470–518: `int sem_types_are_equal(VarType a, VarType b)` compares five structural fields (`base`, `array_size`, `array_depth`, `ptr_depth`, `is_unsigned`) and, for `TYPE_CLASS`/`TYPE_ENUM`/`TYPE_NAMESPACE`, performs string mangling and `streq` checks over `class_name`.
   - `src/semantic/table.c` lines 521–580: `bool sem_types_are_compatible(SemanticCtx *ctx, VarType dest, VarType src)` checks inheritance, traits, void pointers, numeric types, and string compatibility.

3. **Union Synthetic Types & Two-Pass Matching Rules**:
   - `src/semantic/type.c` lines 428 & 436:
     ```c
     // Pass 1: exact match
     if (f->kind == SYM_VAR && sem_types_are_equal(f->type, rhs_type)) { ... }
     // Pass 2: compatible fallback match
     if (f->kind == SYM_VAR && sem_types_are_compatible(ctx, f->type, rhs_type)) { ... }
     ```
   - `src/semantic/check.c` lines 519 & 537: First pass exact match using `sem_types_are_equal`, second pass compatible match using `sem_types_are_compatible`.
   - `src/semantic/fragment/switch.c` lines 223 & 241: First pass exact match using `sem_types_are_equal`, second pass compatible match using `sem_types_are_compatible`.

4. **Target Interface Contracts**:
   - `PROJECT.md` line 30–35:
     ```c
     typedef struct VarType VarType;
     VarType* get_canonical_type(VarTypeKind kind, ...);
     bool types_are_equal(const VarType* a, const VarType* b); // pointer equality a == b
     ```

5. **User Rules & Quality Constraints**:
   - String Comparison Rule: No `strcmp` for string comparisons inside compiler code. Must use `streq` / pointer equality `==`.
   - Debug Print Rule: Every debug print must use a module-specific debug macro (`debug_parser`, `debug_semantic`, `debug_alir`, `debug_lexer`, `debug_codegen`), NEVER raw `printf` or `fprintf(stderr, ...)`.

---

## 2. Logic Chain

1. **From Pass-by-Value to Canonical Pointer System**:
   - Observations 1 & 4 show that replacing `VarType` value structures with `VarType*` canonical pointers requires introducing `include/common/types.h` and `src/common/types.c` with a hash-consing interning pool `get_canonical_type(...)`.
   - Hash-consing guarantees that any two `VarType` requests with identical fields return the exact same memory address (`VarType*`).

2. **From Structural Comparison to Pointer Equality $O(1)$**:
   - Observations 2 & 4 confirm that once types are canonicalized, `types_are_equal(a, b)` and `sem_types_are_equal(a, b)` simplify to `return a == b;`.
   - This eliminates string mangling, allocations, and deep field comparisons during type checking.

3. **Union Synthetic Types Two-Pass Preservation**:
   - Observation 3 specifies the exact union field matching logic (exact match first via `a == b`, then compatible match fallback). Refactoring to `VarType*` preserves this exact ordering while accelerating Pass 1 to a single $O(1)$ pointer comparison.

4. **Codebase-Wide Propagation**:
   - Updating AST nodes (`include/parser/typestruct.h`), symbol tables (`include/semantic/typestruct.h`), ALIR (`include/alir/alir.h`), and codegen to hold `VarType*` allows type assignments to be simple pointer copies.

---

## 3. Caveats

1. **Bidirectional Type Checking (M2)**:
   - This handoff focuses strictly on M1 (String Interning & Canonical `VarType*` Pointers). Bidirectional type checking (`sem_check_expr(node, scope, expected_type)`) will be layered on top in M2.
2. **ALIR Binary I/O**:
   - `src/alir/binary_write.c` and `src/alir/binary_read.c` serialize type tags to disk. On read, `br_type` must reconstruct types via `get_canonical_type(...)` to maintain canonical pointer uniqueness in memory.
3. **No Uninterned Types**:
   - Raw `VarType` structs created on stack must be passed through `get_canonical_type` before assignment to AST nodes or symbols to avoid breaking pointer equality `a == b`.

---

## 4. Conclusion

The refactoring of Alkyl's type system to canonical `VarType*` pointers is fully mapped and ready for implementation:
- `include/common/types.h` and `src/common/types.c` will provide `get_canonical_type(...)` and pre-allocated primitive singletons (`g_type_int`, `g_type_void`, etc.).
- `types_are_equal` and `sem_types_are_equal` will evaluate `a == b` in $O(1)$ time.
- Detailed file-by-file refactoring steps are documented in `/home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_m1_2/analysis.md`.

---

## 5. Verification Method

1. **Build Verification**:
   - Run `make` from the project root to ensure all modules (`src/lexer`, `src/parser`, `src/semantic`, `src/alir`, `src/codegen_llvm`, `src/codegen_qbe`, `src/driver`) compile cleanly without warnings or errors.
2. **Test Suite Verification**:
   - Run `scripts/run_tests.sh` to verify that all test cases pass.
3. **Pointer Equality & Debug Compliance Spot-Check**:
   - Inspect `src/semantic/table.c` to confirm `sem_types_are_equal` uses `a == b`.
   - Grep for `strcmp` across `src/` to confirm no invalid string comparisons remain.
   - Grep for `printf` across `src/` to confirm all debug logging uses `debug_*` macros.
