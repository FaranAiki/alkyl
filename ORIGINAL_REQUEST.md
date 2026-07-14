# Original User Request

## Initial Request — 2026-07-14T14:37:48Z

Extend the Alkyl programming language compiler with advanced class instantiation, optional arguments, named arguments, and lexer settings. The project involves modifying the Lexer, Parser, Semantic Analyzer, and ALIR generation components.

Working directory: /home/faranaiki/Git/alkyl
Integrity mode: development

## Requirements

### R1. Advanced Object Instantiation & Constructors
- Support struct-like instantiation using attributes when an explicit `init()` is not defined.
- Support flexible constructor definition syntax: `init(...) { ... }`, `void init(...) { ... }`, or C-style `ClassName(...) { ... }`.

### R2. Optional/Default Variables and Named Arguments
- Support default argument values in function and method declarations (e.g., `init(int a, int b = 5)`).
- Support named arguments in function and method calls (e.g., `let t = Person(name = "Faran Aiki")`).

### R3. Lexer Settings
- Add a new lexer setting `double_quote_as_string` (defaulting to true).
- When true, string literals like `"..."` automatically parse as `string(c"...")`, while C-strings use `c"..."`.

## Acceptance Criteria

### Verification (Programmatic)
- [ ] Running `make -j4` completes successfully without build errors.
- [ ] A test script `test_constructor.aky` compiles and executes correctly, demonstrating:
  - Implicit struct-like initialization when no `init()` exists.
  - Custom constructors using `init(...)`, `void init(...)`, and `ClassName(...)`.
- [ ] A test script `test_args.aky` compiles and executes correctly, demonstrating:
  - Default arguments in functions.
  - Named arguments in function calls.
- [ ] A test script `test_lexer.aky` compiles and executes correctly, demonstrating that `"..."` expands to `string(c"...")` when `double_quote_as_string` is active.
