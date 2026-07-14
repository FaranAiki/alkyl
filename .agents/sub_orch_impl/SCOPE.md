# Scope: Implementation Track

## Architecture
- **Lexer**: Tokenizes double quotes as strings when the config flag `double_quote_as_string` is active; falls back to default string parsing if not.
- **Parser**: Extends AST nodes for function declarations to support default/optional argument expressions and function calls to support named arguments. Implements advanced constructors (custom init, void init, and class-name constructors) and struct-like instantiation.
- **Semantic Analyzer**: Validates types for default arguments and matches/checks named arguments in function calls. Handles resolution of custom/advanced constructors.
- **ALIR / Codegen**: Lowers default and named arguments, handles constructor call generation and object creation in LLVM.

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| 1 | Lexer Settings | R3: double_quote_as_string setting and string parsing fallback | None | IN_PROGRESS (Conv: d192f686-f8cc-4a53-a0b6-17968979b14e) |
| 3 | Optional/Default Args | R2: Optional/default arguments in function declarations | M1 | PLANNED |
| 4 | Named Arguments | R2: Named arguments in function calls | M3 | PLANNED |
| 5 | Advanced Constructors | R1: Advanced constructors and struct-like instantiation | M4 | PLANNED |
| 6 | E2E Integration & Verification | Pass E2E test suite and adversarial hardening | M1, M3, M4, M5 | PLANNED |

## Interface Contracts
- **Lexer Settings**: Global or context-linked config flag `double_quote_as_string` toggle for double-quoted literals.
- **Default Argument AST**: Parameter nodes extended with optional initialization expression (`ASTNode* default_val`).
- **Named Argument AST**: Call argument nodes extended to optionally associate a param name string (`char* name`).
- **Advanced Constructors**: Functions with name `init`, `void init` (with void return), or matches class name. Supports struct-like constructor calls when no explicit constructor is defined.
