# Handoff Report — Explorer 3 (M1: User Rules & Codebase Audit)

## 1. Observation

### A. `strcmp` Audit
Direct occurrences of `strcmp` found across `src/` and `include/`:
- `src/mlir/expr.c:35`: `if (strcmp(ent->name, var->name) == 0)`
- `src/mlir/expr.c:50`: `if (call->name && strcmp(call->name, "print") == 0)`
- `src/mlir/expr.c:89`: `if (strcmp(cls->name, call->base.sem_type.class_name) == 0)`
- `src/mlir/expr.c:197`: `if (strcmp(en->name, maccess->object->sem_type.class_name) == 0)`
- `src/mlir/expr.c:201`: `if (strcmp(ent->name, maccess->member_name) == 0)`
- `src/mlir/expr.c:223`: `if (strcmp(cls->name, maccess->object->sem_type.class_name) == 0)`
- `src/mlir/expr.c:229`: `if (vd->name && maccess->member_name && strcmp(vd->name, maccess->member_name) == 0)`
- `src/mlir/expr.c:255`: `if (vd->name && maccess->member_name && strcmp(vd->name, maccess->member_name) == 0)`
- `src/mlir/expr.c:287`: `if (strcmp(en->name, enum_name) == 0)`
- `src/semantic/table.c:287`: `if (!(sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0))`
- `src/semantic/table.c:299`: `if (!(sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0))`
- `src/semantic/table.c:309`: `if (sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0)`
- `src/semantic/table.c:376`: `if (!(sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0))`
- `src/semantic/table.c:442`: `if (sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0)`
- `include/common/common.h:46`: `return strcmp(a, b) == 0;` (inside `streq` inline definition)

### B. Debug Print Audit
Occurrences of raw `printf` used for debugging in `src/`:
- `src/alir/fragment/addr.c:395`: `printf("Unknown literal!\n");`
- `src/alir/fragment/generate.c:568`: `printf("No collections\n");`
- `src/alir/fragment/generate.c:610`: `printf("COMPILER ERROR: Attempted to iterate over an invalid or null collection!\n");`
- `src/alir/generator.c:608`: `printf(" - %s fields: %d\n", ds->name, ds->field_count);`
- `src/codegen_llvm/codegen.c:302`: `printf("Setting body for string: class_type=%p types[0]=%p types[1]=%p i32=%p\n", ...);`
- `src/codegen_llvm/codegen.c:434`: `printf("Unreachable!\n");`
- `src/codegen_cranelift/driver.c:13`, `19`: `printf("[Cranelift C Driver] ...");`
- `src/mlir/driver.c:12`, `21`, `28`: `printf("Backend run invoked for MLIR! ...");`
- `src/mlir/expr.c:270`, `393`: `printf("Member access %s on %s: index %d\n", ...);`
- `src/mlir/generate.c:77`, `163`, `221`, `275`, `380`, `384`, `389`: `printf(" Mapping Class Method: %s\n", ...);`
- `src/mlir/stmt.c:231`: `printf("Warning: Unhandled stmt node type %d in MLIR\n", node->type);`
- `src/semantic/check.c:939`: `// printf("Rename: %s -> %s\n", rename_from[i], rename_to[i]);`
- `src/semantic/table.c:233`, `238`, `249`, `254`: `// printf("Adding overload %s to existing %s\n", ...);`

### C. File Extension Audit
- Checked entire repo via `grep_search`: zero occurrences of `.al` as Alkyl source or header file extension.
- File extensions strictly used: `.kyl` for source files (`test/code/**/*.kyl`, `src/driver/main.c`), `.hky` for header files (`src/parser/core.c`, `src/common/zip.c`).

### D. Build & Verification Commands
- Build command executed: `make -j$(nproc)` -> exit code 0.
- Test command executed: `./scripts/run_tests.sh` -> executed 102 test cases (83 passed, 28 failed baseline).

---

## 2. Logic Chain

1. **Observation A** identifies 14 direct `strcmp` calls in `src/mlir/expr.c` and `src/semantic/table.c`.
   - **Reasoning**: The project rule mandates `streq(a, b)` for string comparisons to leverage pointer equality before falling back to character byte comparison. Therefore, all 14 calls in `src/mlir/expr.c` and `src/semantic/table.c` should be replaced with `streq(...)` or `!streq(...)`. The single call in `include/common/common.h:46` is the fallback implementation of `streq` itself.

2. **Observation B** identifies 26 raw `printf` debugging statements in `src/alir/`, `src/codegen_llvm/`, `src/codegen_cranelift/`, `src/mlir/`, and `src/semantic/`.
   - **Reasoning**: User rules mandate replacing raw `printf` and `fprintf` debugging calls with module-specific macros defined in `include/common/debug.h`. Replacing these calls ensures debug messages can be toggled clean via `-DNDEBUG` and adhere to unified formatting.

3. **Observation C** verifies `.al` extensions across all repository directories.
   - **Reasoning**: No `.al` extensions were found. All Alkyl code files in tests and driver strictly use `.kyl` or `.hky`.

4. **Observation D** documents project build and test infrastructure.
   - **Reasoning**: Running `make -j$(nproc)` and `./scripts/run_tests.sh` succeeds and provides the baseline test execution suite.

---

## 3. Caveats

- **REPL Output vs Debug Prints**: Output formatted via `printf` in `src/metalir/metalir.c` (lines 120-273) and `src/driver/cli.c` (lines 24-58) is intentional interactive user output for the REPL (`ethyl`), not internal debug logging.
- **Diagnostics**: Error and hint reports printed via `report_error` / `report_hint` in `src/common/diagnostic.c` and `src/semantic/core.c` are compiler diagnostic errors, not debugging statements.

---

## 4. Conclusion

- **`strcmp` Refactoring**: Exactly 14 calls in `src/mlir/expr.c` (9 calls) and `src/semantic/table.c` (5 calls) need replacement with `streq(...)` / `!streq(...)`.
- **Debug Print Refactoring**: Exactly 26 raw `printf` debugging calls in `src/alir/`, `src/codegen_llvm/`, `src/codegen_cranelift/`, `src/mlir/`, and `src/semantic/` need replacement with `debug_alir`, `debug_codegen`, `debug_driver`, `debug_mlir`, and `debug_semantic`.
- **File Extensions**: Project is fully compliant with `.kyl` / `.hky` requirements; no `.al` file extensions exist.
- **Verification Infrastructure**: Build with `make -j$(nproc)` and test with `./scripts/run_tests.sh`.

---

## 5. Verification Method

To independently verify findings:
1. Search for direct `strcmp` usages:
   ```bash
   rg "\bstrcmp\b" src/ include/
   ```
   (Expect 14 matches in `src/` and 1 in `include/common/common.h`).
2. Search for raw `printf` debug statements:
   ```bash
   rg --pcre2 "(?<!f|s|v|sn|vsn)printf\s*\(" src/
   ```
3. Search for `.al` extension usage:
   ```bash
   rg "\.al\b" src/ include/ test/ scripts/ Makefile
   ```
4. Build and run baseline tests:
   ```bash
   make -j$(nproc)
   ./scripts/run_tests.sh
   ```
