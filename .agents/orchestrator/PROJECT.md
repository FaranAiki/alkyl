# Project: Alkyl Compiler Architecture Refactoring

## Architecture
- **Lexer Module** (`src/lexer/`, `include/lexer/`): Converts source `.kyl`/`.hky` code to tokens. Enhancements: SIMD-accelerated lexing.
- **Common Module** (`src/common/`, `include/common/`): Utility structs and data structures. Enhancements: Global string interner, canonical interned type pointer system, `streq` pointer comparisons.
- **Parser Module** (`src/parser/`, `include/parser/`): Builds AST. Enhancements: Array-of-Structs (AoS) flattened AST representation for cache locality.
- **Semantic Module** (`src/semantic/`, `include/semantic/`): Type checking and symbol tables. Enhancements: Bidirectional type checking (`expected_type`), strict traits bound to `SemSymbol*` interfaces, pointer-equality type checking (`==`).
- **ALIR / Codegen Module** (`src/alir/`, `src/codegen/`, `include/alir/`, `include/codegen/`): Lowering AST to Intermediate Representation and LLVM. Enhancements: Per-function parallel ALIR lowering using a thread pool.
- **Driver & Tests** (`src/driver/`, `test/`, `scripts/`): Interactive shell (`ethyl`), compiler CLI (`alkyl`), test suite runner (`scripts/run_tests.sh`).

## Milestones

| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| E2E | E2E Testing Suite & Infra | Opaque-box test suite for all refactoring features & baseline test runner | none | DONE |
| M1 | R1: String Interning & Canonical Types | Global string interner, canonical `VarType*` pointers, pointer `==` comparisons | E2E infra | PLANNED |
| M2 | R2: Bidirectional Type Checking & Traits | `sem_check_expr(..., expected_type)`, `SemSymbol*` strict traits | M1 | PLANNED |
| M3 | R3a: AoS AST & SIMD Lexing | AoS flattened AST structure, SIMD-accelerated token scanning | M2 | PLANNED |
| M4 | R3b: Parallel ALIR Lowering | Multi-threaded per-function ALIR lowering using worker thread pool | M3 | PLANNED |
| M5 | E2E Test Suite Pass & Adversarial Hardening | Pass 100% test suite, verify pointer comparison, adversarial testing & Forensic Audit | M4, E2E | PLANNED |

## Interface Contracts

### Global String Interner (`include/common/intern.h`)
```c
const char* intern_string(const char* str);
const char* intern_string_len(const char* str, size_t len);
```

### Canonical Type System (`include/common/types.h`)
```c
typedef struct VarType VarType;
VarType* get_canonical_type(VarTypeKind kind, ...);
bool types_are_equal(const VarType* a, const VarType* b); // pointer equality a == b
```

### Bidirectional Type Checker (`include/semantic/semantic.h`)
```c
VarType* sem_check_expr(ASTNode* node, Scope* scope, VarType* expected_type);
```

### Strict Trait Interface (`include/semantic/trait.h`)
```c
typedef struct SemSymbol SemSymbol;
typedef struct TraitBinding {
    SemSymbol* trait_sym;
    SemSymbol* method_sym;
} TraitBinding;
```

## Code Layout
- Sources: `src/{lexer, parser, semantic, alir, codegen, common, driver}`
- Headers: `include/{lexer, parser, semantic, alir, codegen, common, driver}`
- Tests: `test/` (`scripts/run_tests.sh`, `scripts/run_single.sh`)
- Executables: `alkyl` (AOT compiler), `ethyl` (REPL/JIT)
