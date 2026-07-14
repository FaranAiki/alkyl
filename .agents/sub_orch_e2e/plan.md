# E2E Test Track Plan

This plan details the steps to design, implement, and run the E2E test suite for the Alkyl compiler extensions.

## Goal
Design and implement a comprehensive opaque-box E2E test suite for the Alkyl compiler project covering advanced class instantiation, optional arguments, named arguments, and lexer settings, and publish `TEST_READY.md` and `TEST_INFRA.md`.

## 4-Tier Test Suite Design

### Feature List (N = 4)
1. **F1: Lexer Settings (`double_quote_as_string`)**
2. **F2: Default/Optional Arguments**
3. **F3: Named Arguments**
4. **F4: Advanced Object Instantiation & Constructors**

### Tier 1: Feature Coverage (>= 5 cases per feature)
- **F1 (Lexer Settings)**:
  1. Default behavior (should be true, `"..."` is `string(c"...")`).
  2. Enabling setting explicitly.
  3. Disabling setting explicitly (if supported/how to disable, checking that `"..."` behaves as standard C-string `char*`).
  4. Explicit `c"..."` C-string parsing under both settings.
  5. Multi-line or escaped double quotes under the string type.
- **F2 (Default Arguments)**:
  1. Single default argument at the end of a function.
  2. Multiple default arguments in function declarations.
  3. Method declaration with default arguments in a class.
  4. Omitting default arguments in function calls (verifying default values are used).
  5. Providing explicit values to override default arguments.
- **F3 (Named Arguments)**:
  1. Calling functions with named arguments only (matching declaration order).
  2. Calling functions with named arguments in shuffled/non-declaration order.
  3. Combining positional and named arguments (positional first, then named).
  4. Calling class methods with named arguments.
  5. Calling a function with multiple arguments where some are passed as named and some use defaults.
- **F4 (Advanced Object Instantiation & Constructors)**:
  1. Class instantiation with implicit constructor (struct-like instantiation by setting attributes when no `init` is defined).
  2. Custom constructor `init(...) { ... }`.
  3. Custom constructor `void init(...) { ... }`.
  4. Custom constructor C-style `ClassName(...) { ... }`.
  5. Class with multiple overloaded custom constructors.

### Tier 2: Boundary & Corner Cases (>= 5 cases per feature)
- **F1 (Lexer Settings)**:
  1. Empty string `""` under `double_quote_as_string`.
  2. String literal containing escaped characters under both settings.
  3. Very long string literal.
  4. Invalid character escape sequences inside string literals.
  5. Mixed type concatenations / operations on the parsed `string` type.
- **F2 (Default Arguments)**:
  1. Default argument referencing another variable or expression (if supported, e.g. constant expression).
  2. Boundary: all arguments having default values.
  3. Default argument values at limits (e.g. max integer value, empty string).
  4. Default argument that is a null pointer.
  5. Overloading functions that differ only in default arguments (verifying compiler resolution or clear error).
- **F3 (Named Arguments)**:
  1. Passing the same named argument multiple times (compiler error).
  2. Named argument that does not exist in the function signature (compiler error).
  3. Positional argument passed *after* a named argument (compiler error or correct resolution boundary).
  4. Shadowing: named argument name identical to a local variable name at the call site.
  5. Named argument with complex expression as value.
- **F4 (Advanced Object Instantiation & Constructors)**:
  1. Struct-like instantiation with too few/too many arguments (compiler error).
  2. Struct-like instantiation with incorrect types for attributes (compiler error).
  3. Constructor `init` returning a non-void value when not specified as `void` (if compiler checks).
  4. Nested objects: struct-like instantiation of a class that has another class as an attribute.
  5. Instantiation of a class with cyclic constructor dependencies or recursive instantiation.

### Tier 3: Cross-Feature Combinations (>= 4 cases)
1. **F1 + F2 + F3**: Default arguments and named arguments called with a double-quoted string literal (which parses as `string` due to lexer setting).
2. **F2 + F3 + F4**: Custom constructor called using both default and named arguments.
3. **F1 + F4**: Struct-like instantiation where attribute types are `string` (parsed from double quotes).
4. **F2 + F3 + F4**: Struct-like instantiation where attributes have default values (if supported) or constructor uses default and named arguments.

### Tier 4: Real-World Application Scenarios (>= 5 cases)
1. **Config Loader**: A configuration loader using a class `Config` with default arguments for server settings, initialized via named arguments, parsing string options.
2. **Database Query Builder**: Building SQL queries using a class `Query` with methods that take optional/default options and named parameters.
3. **GUI Component / Widget**: Designing a `Button` class with options like `width`, `height`, `label` (using `string` from double-quotes), utilizing custom constructors.
4. **HTTP Request Client**: Making an HTTP-like client module that builds requests with optional parameters (headers, timeout, method) and uses named arguments to set them.
5. **Simple Game Entity System**: An entity manager where players and enemies are instantiated using struct-like syntax or custom constructors, handling default stats (health, speed).

## Steps to Execute
1. **Phase 1: Scope Setup & Decompose**: Write `SCOPE.md`.
2. **Phase 2: Test Harness & Infra Design**: Determine where tests are written. Set up directory `/home/faranaiki/Git/alkyl/test/code` and create a custom test runner script or integrate with the existing `scripts/run_tests.sh` (or create a wrapper `test_runner.py` that is robust and runs the test suite).
3. **Phase 3: Write Test Cases**: Dispatch a worker to write the `.aky` test files and their expected `.out` / `.log` / `.in` files.
4. **Phase 4: Run & Verify**: Run tests and verify the implementation under development.
5. **Phase 5: Adversarial Hardening**: Generate Tier 5 test cases or refine test coverage.
6. **Phase 6: Publish & Report**: Publish `TEST_READY.md` and `TEST_INFRA.md` and report success.
