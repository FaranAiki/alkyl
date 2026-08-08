# Codebase Violation Audit & Analysis — Milestone 1 (M1)

## Executive Summary
This document provides a comprehensive audit of the Alkyl compiler codebase (`src/`, `include/`) for compliance with key project user rules:
1. Replaces all direct `strcmp` string comparisons with `streq` or pointer equality `==`.
2. Identifies all raw `printf` / `fprintf` / `sprintf` statements used for debugging and maps them to their module-specific debug macro (`debug_lexer`, `debug_parser`, `debug_semantic`, `debug_alir`, `debug_codegen`, `debug_mlir`, `debug_driver`, `debug_any`).
3. Verifies file extension handling across the codebase to ensure `.kyl` (source) and `.hky` (header) are strictly used/assumed and no `.al` extensions exist.
4. Documents exact build and test steps for verification.

---

## 1. `strcmp` Audit & Refactoring Specifications

**User Rule**: Do NOT use `strcmp` for string comparisons inside the compiler codebase. ALWAYS use the inline function `streq(const char *a, const char *b)` defined in `include/common/common.h` which performs pointer equality first for speed.

### Audit Findings
Total occurrences of `strcmp` found in codebase: **15** (14 in `src/`, 1 in `include/common/common.h`).

| # | File Path | Line | Current Code | Refactored Replacement |
|---|-----------|------|--------------|------------------------|
| 1 | `src/mlir/expr.c` | 35 | `if (strcmp(ent->name, var->name) == 0)` | `if (streq(ent->name, var->name))` |
| 2 | `src/mlir/expr.c` | 50 | `if (call->name && strcmp(call->name, "print") == 0)` | `if (call->name && streq(call->name, "print"))` |
| 3 | `src/mlir/expr.c` | 89 | `if (strcmp(cls->name, call->base.sem_type.class_name) == 0)` | `if (streq(cls->name, call->base.sem_type.class_name))` |
| 4 | `src/mlir/expr.c` | 197 | `if (strcmp(en->name, maccess->object->sem_type.class_name) == 0)` | `if (streq(en->name, maccess->object->sem_type.class_name))` |
| 5 | `src/mlir/expr.c` | 201 | `if (strcmp(ent->name, maccess->member_name) == 0)` | `if (streq(ent->name, maccess->member_name))` |
| 6 | `src/mlir/expr.c` | 223 | `if (strcmp(cls->name, maccess->object->sem_type.class_name) == 0)` | `if (streq(cls->name, maccess->object->sem_type.class_name))` |
| 7 | `src/mlir/expr.c` | 229 | `if (vd->name && maccess->member_name && strcmp(vd->name, maccess->member_name) == 0)` | `if (vd->name && maccess->member_name && streq(vd->name, maccess->member_name))` |
| 8 | `src/mlir/expr.c` | 255 | `if (vd->name && maccess->member_name && strcmp(vd->name, maccess->member_name) == 0)` | `if (vd->name && maccess->member_name && streq(vd->name, maccess->member_name))` |
| 9 | `src/mlir/expr.c` | 287 | `if (strcmp(en->name, enum_name) == 0)` | `if (streq(en->name, enum_name))` |
| 10 | `src/semantic/table.c` | 287 | `if (!(sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0))` | `if (!(sym->is_private && ctx->current_filename && sym->filename && !streq(ctx->current_filename, sym->filename)))` |
| 11 | `src/semantic/table.c` | 299 | `if (!(sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0))` | `if (!(sym->is_private && ctx->current_filename && sym->filename && !streq(ctx->current_filename, sym->filename)))` |
| 12 | `src/semantic/table.c` | 309 | `if (sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0)` | `if (sym->is_private && ctx->current_filename && sym->filename && !streq(ctx->current_filename, sym->filename))` |
| 13 | `src/semantic/table.c` | 376 | `if (!(sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0))` | `if (!(sym->is_private && ctx->current_filename && sym->filename && !streq(ctx->current_filename, sym->filename)))` |
| 14 | `src/semantic/table.c` | 442 | `if (sym->is_private && ctx->current_filename && sym->filename && strcmp(ctx->current_filename, sym->filename) != 0)` | `if (sym->is_private && ctx->current_filename && sym->filename && !streq(ctx->current_filename, sym->filename))` |
| 15 | `include/common/common.h` | 46 | `return strcmp(a, b) == 0;` | Intentionally retained inside `streq` implementation as pointer fallback. |

---

## 2. Debug Print Audit (`printf` / `fprintf` / `sprintf`)

**User Rule**: Every `fprintf` or `printf` that is used for debugging MUST be replaced by a module-specific debug macro (`debug_lexer`, `debug_parser`, `debug_semantic`, `debug_alir`, `debug_codegen`, `debug_mlir`, `debug_driver`, `debug_any`). Do NOT use raw `printf("DEBUG: ...")` or `fprintf(stderr, ...)`.

### Audit Findings
Below are all occurrences of raw debugging print statements in `src/` and `include/` that require replacement with debug macros:

| # | File Path | Line | Current Debug Print | Suggested Debug Macro Replacement |
|---|-----------|------|---------------------|-----------------------------------|
| 1 | `src/alir/fragment/addr.c` | 395 | `printf("Unknown literal!\n");` | `debug_alir("Unknown literal!\n");` |
| 2 | `src/alir/fragment/generate.c` | 568 | `printf("No collections\n");` | `debug_alir("No collections\n");` |
| 3 | `src/alir/fragment/generate.c` | 610 | `printf("COMPILER ERROR: Attempted to iterate over an invalid or null collection!\n");` | `debug_alir("COMPILER ERROR: Attempted to iterate over an invalid or null collection!\n");` |
| 4 | `src/alir/generator.c` | 608 | `printf(" - %s fields: %d\n", ds->name, ds->field_count);` | `debug_alir(" - %s fields: %d\n", ds->name, ds->field_count);` |
| 5 | `src/codegen_llvm/codegen.c` | 302 | `printf("Setting body for string: class_type=%p types[0]=%p types[1]=%p i32=%p\n", ...);` | `debug_codegen("Setting body for string: class_type=%p types[0]=%p types[1]=%p i32=%p\n", ...);` |
| 6 | `src/codegen_llvm/codegen.c` | 434 | `printf("Unreachable!\n");` | `debug_codegen("Unreachable!\n");` |
| 7 | `src/codegen_cranelift/driver.c` | 13 | `printf("[Cranelift C Driver] Invoking Rust cranelift backend...\n");` | `debug_driver("[Cranelift C Driver] Invoking Rust cranelift backend...\n");` |
| 8 | `src/codegen_cranelift/driver.c` | 19 | `printf("[Cranelift C Driver] Linking with: %s\n", cmd);` | `debug_driver("[Cranelift C Driver] Linking with: %s\n", cmd);` |
| 9 | `src/mlir/driver.c` | 12 | `printf("Backend run invoked for MLIR! (Delegating to mlir_generate...)\n");` | `debug_mlir("Backend run invoked for MLIR! (Delegating to mlir_generate...)\n");` |
| 10 | `src/mlir/driver.c` | 21 | `printf("MLIR lowering to LLVM IR failed\n");` | `debug_mlir("MLIR lowering to LLVM IR failed\n");` |
| 11 | `src/mlir/driver.c` | 28 | `printf("Clang compilation failed\n");` | `debug_mlir("Clang compilation failed\n");` |
| 12 | `src/mlir/expr.c` | 270 | `printf("Member access %s on %s: index %d\n", ...);` | `debug_mlir("Member access %s on %s: index %d\n", ...);` |
| 13 | `src/mlir/expr.c` | 393 | `printf("Warning: Unhandled expr node type %d in MLIR\n", node->type);` | `debug_mlir("Warning: Unhandled expr node type %d in MLIR\n", node->type);` |
| 14 | `src/mlir/generate.c` | 77 | `printf("  Mapping Class Method: %s\n", func_name);` | `debug_mlir("  Mapping Class Method: %s\n", func_name);` |
| 15 | `src/mlir/generate.c` | 163 | `printf("  Mapping Class Method: %s\n", func_name);` | `debug_mlir("  Mapping Class Method: %s\n", func_name);` |
| 16 | `src/mlir/generate.c` | 221 | `printf("  Mapping FuncDefNode: %s\n", fn->name);` | `debug_mlir("  Mapping FuncDefNode: %s\n", fn->name);` |
| 17 | `src/mlir/generate.c` | 275 | `printf("  Mapping Class Method: %s\n", func_name);` | `debug_mlir("  Mapping Class Method: %s\n", func_name);` |
| 18 | `src/mlir/generate.c` | 380 | `printf("Starting MLIR AST-lowering for %s...\n", basename);` | `debug_mlir("Starting MLIR AST-lowering for %s...\n", basename);` |
| 19 | `src/mlir/generate.c` | 384 | `printf("MLIR is not fully linked/available, or create_context failed.\n");` | `debug_mlir("MLIR is not fully linked/available, or create_context failed.\n");` |
| 20 | `src/mlir/generate.c` | 389 | `printf("Successfully created MLIR context and module from AST via C++ wrapper!\n");` | `debug_mlir("Successfully created MLIR context and module from AST via C++ wrapper!\n");` |
| 21 | `src/mlir/stmt.c` | 231 | `printf("Warning: Unhandled stmt node type %d in MLIR\n", node->type);` | `debug_mlir("Warning: Unhandled stmt node type %d in MLIR\n", node->type);` |
| 22 | `src/semantic/check.c` | 939 | `// printf("Rename: %s -> %s\n", rename_from[i], rename_to[i]);` | `// debug_semantic("Rename: %s -> %s\n", rename_from[i], rename_to[i]);` |
| 23 | `src/semantic/table.c` | 233 | `// printf("Adding overload %s to existing %s\n", ...);` | `// debug_semantic("Adding overload %s to existing %s\n", ...);` |
| 24 | `src/semantic/table.c` | 238 | `// printf("Adding new symbol %s\n", sym->name);` | `// debug_semantic("Adding new symbol %s\n", sym->name);` |
| 25 | `src/semantic/table.c` | 249 | `// printf("Adding overload %s to existing %s\n", ...);` | `// debug_semantic("Adding overload %s to existing %s\n", ...);` |
| 26 | `src/semantic/table.c` | 254 | `// printf("Adding new symbol %s\n", sym->name);` | `// debug_semantic("Adding new symbol %s\n", sym->name);` |

---

## 3. File Extensions Audit (`.kyl` & `.hky`)

**User Rule**: Alkyl files use the extensions `.kyl` (source) and `.hky` (header) strictly. Do NOT use or assume `.al` extensions.

### Audit Findings
- **`.al` Extension Absence**: Scanned full repository (`src/`, `include/`, `test/`, `scripts/`, `Makefile`). Zero occurrences of `.al` exist as source/header extensions.
- **Source File Extensions**: `.kyl` is used as standard Alkyl source file extension (`test/code/**/*.kyl`, `src/driver/main.c`, `src/driver/cli.c`).
- **Header File Extensions**: `.hky` is registered as standard Alkyl header file extension.
- **Import Resolution**: `src/parser/core.c` line 929 checks `exts[] = { ".kyl", ".hky", ".alk", ".alky", ".alkyl", ".aky", ".zyl", "" }`.
- **Zip Archive Handling**: `src/common/zip.c` line 12-17 checks `.kyl`, `.hky`, `.alk`, `.alky`, `.alkyl`, `.aky`.

---

## 4. Build Infrastructure & Test Verification Procedures

### Build Command
```bash
make -j$(nproc)
```
- **Produced Binaries**: `build/alkyl` (main compiler), `build/ethyl` (interactive REPL), `build/alkyl_llvm`, `build/alkyl_qbe`, `build/alkyl_mlir`, `build/alkyl_cranelift`.

### Test Command
```bash
./scripts/run_tests.sh
# Or parallel execution:
./scripts/run_tests.sh --parallel
```
- **Test Baseline Result**: 83 Passed, 28 Failed out of 102 total test cases on default build.
