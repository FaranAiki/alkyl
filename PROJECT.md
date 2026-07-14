# Project: Alkyl Compiler Extensions

## Architecture
Alkyl Compiler consists of:
- Lexer: Tokenization of source files.
- Parser: AST construction using recursive descent.
- Semantic Analyzer: Scope resolution, type checking, method and overload resolution.
- ALIR: Intermediate representation generation from AST.
- LLVM Codegen: Emitting LLVM IR.

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| 1 | E2E Testing Track | Develop test suite Tiers 1-4 | None | IN_PROGRESS (Conv: 8a6336b3-35e7-4d3f-8134-b9c63cc2987b) |
| 2 | Lexer Settings | R3: double_quote_as_string setting | None | IN_PROGRESS (Conv: 984ccd4d-afad-4943-952c-f0c6e85707cc) |
| 3 | Optional/Default Args | R2: Default args in function/method declarations | M2 | PLANNED |
| 4 | Named Arguments | R2: Named arguments in call sites | M3 | PLANNED |
| 5 | Advanced Constructors | R1: Custom init/void init/ClassName constructors and struct-like instantiation | M4 | PLANNED |
| 6 | E2E Integration & Verification | Pass E2E test suite and adversarial hardening | M1, M5 | PLANNED |

## Interface Contracts
- Lexer Settings: A global or context-linked configuration flag `double_quote_as_string`.
- Default Argument AST: Function parameter structures extended with an optional expression pointer representing the default value.
- Named Argument AST: Function call argument structures extended to link a name string with an expression.
- Constructors: Function names in classes mapped to the constructor logic if named `init`, `void init` (ret type void), or the class name itself.
