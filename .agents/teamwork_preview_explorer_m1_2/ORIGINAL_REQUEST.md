## 2026-08-08T00:11:26Z
<USER_REQUEST>
You are Explorer 2 for Milestone 1 (M1: String Interning & Canonical Type Pointers).
Your working directory is: /home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_m1_2
Project Document: /home/faranaiki/Git/alkyl/.agents/orchestrator/PROJECT.md
Scope Document: /home/faranaiki/Git/alkyl/.agents/sub_orch_impl/SCOPE.md

Your task:
1. Investigate the codebase for refactoring `VarType` to a canonical `VarType*` pointer system:
   - `include/common/types.h` and `src/common/types.c`
   - Interface: `VarType* get_canonical_type(VarTypeKind kind, ...);` and `bool types_are_equal(const VarType* a, const VarType* b);` (which evaluates `a == b`).
   - Examine how `VarType` structures are allocated, copied, compared, and passed in AST nodes, symbol tables, parser, semantic analyzer (`src/semantic/`), and codegen (`src/codegen/`).
2. Identify all `strcmp` or deep structural type comparison functions that currently compare types, and map how they will be refactored to single `VarType*` pointer equality `a == b`.
3. Check union synthetic types (`__Union_...`) and two-pass matching rules specified in AGENTS.md (exact match first, then compatible match fallback).
4. Verify user rules: no `strcmp`, use `streq` / pointer equality `==`. All debug prints must use debug macros (`debug_*`), NEVER raw `printf`/`fprintf`.
5. Write your findings and proposed implementation plan to `/home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_m1_2/analysis.md` and your handoff report to `/home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_m1_2/handoff.md`.
6. Send a message to parent with the path to your handoff report.
</USER_REQUEST>
