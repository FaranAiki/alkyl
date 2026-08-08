## 2026-08-08T00:14:01Z
You are the Worker for Milestone 1 (M1: R1 String Interning & Canonical Type Pointers).
Your working directory is: /home/faranaiki/Git/alkyl/.agents/teamwork_preview_worker_m1_1
Project Document: /home/faranaiki/Git/alkyl/.agents/orchestrator/PROJECT.md
Scope Document: /home/faranaiki/Git/alkyl/.agents/sub_orch_impl/SCOPE.md
Domain Skill Path: /home/faranaiki/Git/alkyl/.agents/skills/alkyl-language/SKILL.md

Refer to the exploration handoff reports:
- Explorer 1 Report: /home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_m1_1/handoff.md (and analysis.md)
- Explorer 2 Report: /home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_m1_2/handoff.md (and analysis.md)
- Explorer 3 Report: /home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_m1_3/handoff.md (and analysis.md)

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A Forensic Auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

Your Tasks:
1. **Global String Interner (`include/common/intern.h`, `src/common/intern.c`)**:
   - Implement `const char* intern_string(const char* str);` and `const char* intern_string_len(const char* str, size_t len);`.
   - Update `lexer.c`, `parser/core.c` (`parser_strdup`), `semantic/table.c` (`SemSymbol` names), and context allocation to use `intern_string`.
2. **Canonical `VarType*` Pointer System (`include/common/types.h`, `src/common/types.c`)**:
   - Implement `VarType* get_canonical_type(VarTypeKind kind, ...);` and pre-allocated primitive singletons (`g_type_int`, `g_type_void`, etc.).
   - Implement `bool types_are_equal(const VarType* a, const VarType* b)` and `sem_types_are_equal` evaluating `a == b`.
   - Refactor `VarType` value structs to `VarType*` pointers across `include/parser/typestruct.h`, AST nodes, symbol tables (`SemSymbol.type`, `SemScope.expected_ret_type`), ALIR (`AlirValue.type`, `AlirFunction.ret_type`), and codegen.
   - Maintain the two-pass union type matching logic in `src/semantic/type.c`, `src/semantic/check.c`, and `src/semantic/fragment/switch.c` (Pass 1 exact match `a == b`, Pass 2 compatible match `sem_types_are_compatible`).
3. **User Rules & Codebase Compliance**:
   - Refactor all `strcmp` calls in `src/mlir/expr.c` and `src/semantic/table.c` to `streq(...)` / `!streq(...)`.
   - Refactor raw `printf`/`fprintf` debug calls across `src/alir/`, `src/codegen_llvm/`, `src/codegen_cranelift/`, `src/mlir/`, and `src/semantic/` to use debug macros (`debug_alir`, `debug_codegen`, `debug_driver`, `debug_mlir`, `debug_semantic`, `debug_any`).
   - Ensure strictly `.kyl` and `.hky` file extensions are used for Alkyl code.
4. **Verification**:
   - Build with `make -j$(nproc)`.
   - Run test suite with `./scripts/run_tests.sh`.
   - Document build and test outputs, verification evidence, changed files, and final state in `/home/faranaiki/Git/alkyl/.agents/teamwork_preview_worker_m1_1/handoff.md`.
5. Send a message to parent with the path to your handoff report when complete.
