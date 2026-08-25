Here is the architectural documentation and programming guide for the **Alkyl** compiler project. (Also applicable for Kilo Code and other AI assistants)

<CRITICAL_SYNTAX_RULE>
**Alkyl uses C-like syntax, NOT modern Rust-like syntax.**

Function declarations use C-style: `int main() { }`, NOT Rust-style `main: int`.
Variable declarations use C-style: `int x = 10;` or type-inferred `let x = 10;`.
The `let` keyword is analogous to `auto` in C++ — it infers the type from the initializer.
**`let t: int = 0` is INVALID and makes no sense in Alkyl.** Do NOT use colon-type annotations (`: type`) after variable names.
Always write `int p = 1;` for explicit typing, or `let p = 1;` for type inference.

For goddamnit sake DON'T USE WINDOWS \r\n
Use dos2unix!
</CRITICAL_SYNTAX_RULE>

<CRITICAL_LANGUAGE_FEATURES_RULE>
**Alkyl is C++-inspired for OOP and module features, NOT Rust-like.**

Visibility modifiers (C++-style): `public`, `private`, `open`, `closed`.
- `public`: Accessible everywhere.
- `private`: Accessible only within the declaring scope.
- `open`: Can be subclassed / reopened (for classes and namespaces).
- `closed`: Cannot be subclassed / reopened.

Namespaces use C++-style syntax: `namespace Math { ... }`.
Export namespaces with `export namespace`: `export namespace Math { ... }`.
Classes support single inheritance (`is`) and traits/mixins (`has`).
Functions can be declared `pure` / `impure` and `total` / `partial`.
Variables and expressions use `pristine` / `tainted` for error safety (Zig-inspired, orthogonal to pure/impure).

**`string` is NOT a built-in primitive type.** It is a library class defined in `lib/std/string.kyl` and is NOT fully implemented (contains TODOs). Do NOT treat `string` as a primitive like `int` or `char`. Use `char*` for strings unless explicitly importing the `string` library.
</CRITICAL_LANGUAGE_FEATURES_RULE>

<RULE>
**Debugging Print Rule:**
Every `fprintf` or `printf` that is used for debugging MUST be replaced by a module-specific debug macro (e.g. `debug_parser`, `debug_alir`, `debug_lexer`, `debug_semantic`, `debug_codegen`, `debug_c_header`, `debug_driver`, `debug_alick`, `debug_metalir`, `debug_optlir`, `debug_mlir`) or `debug_any("msg", ...);` if none fit. Do NOT use `printf("DEBUG: ...")` or `fprintf(stderr, ...)`.

Available debug macros (defined in `include/common/debug.h`):
- `debug_parser(msg, ...)` — parser module
- `debug_alir(msg, ...)` — ALIR module
- `debug_alick(msg, ...)` — ALIR checker
- `debug_lexer(msg, ...)` — lexer module
- `debug_codegen(msg, ...)` — codegen module
- `debug_semantic(msg, ...)` — semantic analysis module
- `debug_c_header(msg, ...)` — C header parser
- `debug_driver(msg, ...)` — driver/CLI module
- `debug_metalir(msg, ...)` — MetaVM/JIT module
- `debug_optlir(msg, ...)` — ALIR optimizer
- `debug_mlir(msg, ...)` — MLIR backend
- `debug_any(msg, ...)` — generic fallback

**Doxygen Documentation Rule:**
When adding any new public function declaration to a header file under `include/`, you MUST precede it with a Doxygen-style `/** ... */` block comment containing `@brief`, `@param` for each parameter, and `@return` if the function returns a value. Example:

```c
/**
 * @brief Creates a new ALIR module.
 * @param ctx The compiler context.
 * @param name The module name.
 * @return The new ALIR module.
 */
AlirModule* alir_create_module(CompilerContext *ctx, const char *name);
```

This applies to ALL public headers in `include/`. Internal headers (e.g. `parser_internal.h`) should also document functions where practical. Do NOT add functions without documentation.

**String Comparison Rule:**
Do NOT use `strcmp` for string comparisons inside the compiler codebase. ALWAYS use the inline function `streq(const char *a, const char *b)` defined in `include/common/common.h` which performs pointer equality first for speed.
</RULE>

# How to Program in Alkyl
1. **Variables and Assignment**:
   - Declare variables with `let` (type inference) or C-style explicit typing, e.g., `let p = 1;` or `int p = 1;`
   - In the REPL, `p = 1` also acts as an implicit declaration if `p` doesn't exist.
   - The REPL automatically saves the last evaluated expression in a global variable called `res` (similar to `_` in Python or `it` in GHCi).
2. **Types**:
   - Primitives: `int`, `double`, `single`, `long`, `char`, `bool`.
   - `noreturn`: Function return type indicating the function never returns (e.g. `exit`, `panic`).
   - **`string` is NOT a primitive.** It is a library class in `lib/std/string.kyl` and is NOT fully implemented (contains TODOs). Use `char*` for strings unless explicitly importing the `string` library.
   - Arrays: `[1, 2, 3]` (Array literals are fully supported in REPL).
   - User-defined: `class`, `namespace`, `enum`. Unions are synthetic types.
3. **Control Flow**:
   - Standard `if`, `while`, `switch`.
4. **Functions & Macros**:
   - Functions are defined using C-like syntax WITHOUT a `func` keyword: `int add(int a, int b) { return a + b; }`
   - Visibility modifiers (C++-style): `public`, `private`, `open`, `closed`.
   - Macros are defined with `meta void` and are expanded at the AST level. Never eagerly compile an unexpanded macro to ALIR.
   - Nullability: Alkyl does not use null pointers (e.g. `0 as void*`). Instead, absence of a value is represented as an error state (e.g., `purge ErrNull`).
5. **Namespaces & Modules**:
   - Namespaces use C++-style syntax: `namespace Math { ... }`.
   - Export namespaces with `export namespace`: `export namespace Math { ... }`.
   - Namespaces can be `public`, `private`, `open`, or `closed`.
6. **Object-Oriented Programming**:
   - Classes support single inheritance (`class Player is Entity`) and traits/mixins (`class User has Printable`).
   - Class visibility: `public` (accessible everywhere), `private` (accessible only within declaring scope), `open` (can be subclassed), `closed` (cannot be subclassed).
7. **Effects & Error Handling**:
   - Functions: `pure` / `impure` (side effects), `total` / `partial` (termination).
   - Values: `pristine` / `tainted` (error safety). `tainted` is orthogonal to `pure`/`impure`. Use `wash`, `clean`, `untaint`, or `?` to handle tainted values.
8. **Foreign Function Interface (FFI) & Calling Conventions**:
   - Use `@identifier` before `extern` or `func` to specify calling conventions or name mangling schemas without using pragmas.
   - Example: `@cpp extern { int some_cpp_func(); }` will use C++ Itanium name mangling.
   - Example: `@stdcall int my_func();`
   - Example: `@rust extern int get_rust_data();`

Here is the architectural documentation for the **Alkyl** compiler project.

**CRITICAL RULE:** Always consult `misc/howto.json` for essential information about Alkyl's syntax, design notes (effects like pure/impure/total/partial), error handling (pristine/tainted), generics, and OOP keywords. `howto.json` is the definitive source of truth for Alkyl's language semantics!

---

# Alkyl Compiler Architecture Documentation

## 1. High-Level Overview

**Alkyl** is a statically typed, compiled programming language implemented entirely in **C**. It is designed to be deeply modular and supports multiple interchangeable backends:
- **LLVM C API**: Standard high-performance backend.
- **MLIR**: Advanced, state-of-the-art lowering dialect.
- **QBE**: Minimalist, fast-compiling backend.

The project transforms source code through distinct stages: lexical analysis, parsing (AST construction), semantic analysis (type checking and symbol resolution), and finally code generation to the selected backend.

The system supports two modes of operation:

1. **AOT (Ahead-of-Time) Compilation:** Compiling source files to native object code and linking them into executables.
2. **JIT (Just-In-Time) Execution:** An interactive REPL (`ethyl`) for immediate code evaluation.

## 2. Directory Breakdown (`src/`)

### `src/diagnostic/`

**Responsibility:** User Experience & Error Reporting.
This module abstracts the complexity of formatting error messages and provides "fuzzy" logic to help users correct mistakes.

* **`diagnostic.c` / `diagnostic.h**`:
* **Context Tracking:** Uses `diag_set_namespace` to track where errors occur (e.g., inside specific functions or classes).
* **Visuals:** Defines ANSI color codes (`DIAG_RED`, `DIAG_BOLD`) for terminal output.
* **Fuzzy Matching:** Implements the **Levenshtein Distance** algorithm (`levenshtein_dist`). When a user types an unknown identifier, the compiler scans valid keywords/variables and uses `find_closest_keyword` to offer "Did you mean...?" suggestions.



### `src/lexer/`

**Responsibility:** Tokenization.
Converts raw source code strings into a stream of atomic units called Tokens.

* **`lexer.c`**: Implements a pull-based lexer. It handles white-space skipping, comment removal, and character lookahead. It creates specific tokens for complex operators (e.g., `+=`, `<<=`) and literals (strings, chars, floats).
* **`lexer.h`**: Defines the `TokenType` enum (containing keywords like `TOKEN_KW_MUT`, `TOKEN_CLASS`, operators, and literals) and the `Token` struct which holds the text, line number, and column number.

### `src/parser/`

**Responsibility:** Abstract Syntax Tree (AST) Construction.
This directory implements a **Recursive Descent Parser**. The logic is split into multiple files to maintain maintainability as the grammar grows.

* **`parser.h`**: The public interface. It defines the AST structs (`ASTNode`, `FuncDefNode`, `IfNode`, etc.) used by the rest of the compiler.
* **`parser_internal.h`**: A private header used *only* within `src/parser/` to share internal helper functions (`eat`, `parse_expression`) without exposing them globally.
* **`core.c`**: Handles infrastructure. It manages the token stream, implements synchronization logic (`parser_sync`) to recover from errors, and handles macro registration/expansion.
* **`top.c`**: Parses top-level constructs: namespaces, class definitions, global variables, function definitions, and imports (`import`, `link`).
* **`stmt.c`**: Parses imperative statements: `return`, `if`, `while`, `switch`, and variable declarations.
* **`expr.c`**: Parses expressions using precedence climbing. It handles binary operations, unary operators, function calls, and array access.

### `src/semantic/`

**Responsibility:** Validation & Type Checking.
This is the "gatekeeper" module. It ensures the AST is valid before the backend touches it.

* **`semantic.c`**:
* **Symbol Table:** Manages scopes (`Scope` struct) to track variable definitions, shadowing, and visibility.
* **Type Checking:** The `check_expr` function calculates result types and ensures compatibility (e.g., preventing the addition of a string to a boolean).
* **Overload Resolution:** It performs name mangling (`mangle_function`) and determines which function variant to call based on argument types.
* **Access Control:** Verifies that `this` is used only within methods and checks for access to undefined members.



### `src/codegen/`

**Responsibility:** IR Generation.
Translates the Semantic-validated AST into LLVM Intermediate Representation (IR).

* **`core.c`**: Sets up the LLVM module and builder. It defines mapping from Alkyl types (`VarType`) to LLVM types (`LLVMTypeRef`) and registers built-in C functions (malloc, free, printf).
* **`flow.c`**: Generates IR for control flow structures (`if`, `while`, `switch`) and function definitions. It manages basic blocks (entry, body, end).
* **`expr.c`**: Generates IR for expressions. It handles math instructions (`LLVMBuildAdd`), pointer arithmetic (`LLVMBuildGEP2`), and function calls.
* **`stmt.c`**: Handles memory allocation for variables (`LLVMBuildAlloca`) and store instructions.

### `src/driver/`

**Responsibility:** Application Entry Points.
This directory connects the library modules into executable tools.

* **`main.c` (The Compiler)**:
* Reads a source file.
* Runs Lexer  Parser  Semantic Analysis  Codegen.
* Emits an object file (`out.o`).
* Invokes the system linker (`gcc`) to produce a final executable.


* **`cli.c` (The REPL)**:
* Implements an interactive shell using `readline`.
* Uses **MetaVM** (the internal ALIR interpreter) to compile and execute AST nodes in memory immediately, allowing line-by-line interaction.



---

## 3. Compilation Pipeline Trace

Tracing the input: `let x = 10;`

1. **Lexer (`src/lexer/`)**: Reads the string. Generates tokens:
* `TOKEN_KW_LET`
* `TOKEN_IDENTIFIER ("x")`
* `TOKEN_ASSIGN`
* `TOKEN_NUMBER (10)`
* `TOKEN_SEMICOLON`


2. **Parser (`src/parser/top.c`)**: Detects `let`. Calls `parse_var_decl_internal`. Creates a `VarDeclNode` where `name="x"`, `type=TYPE_AUTO`, and `initializer` is a `LiteralNode(10)`.
3. **Semantic (`src/semantic/`)**: `check_stmt` sees the `VarDeclNode`. It infers the type of `x` is `int` based on the initializer `10`. It adds `x` (type `int`) to the current scope's symbol table.
4. **Codegen (`src/codegen/stmt.c`)**: `codegen_var_decl` is called.
* It creates an `alloca` instruction (stack memory) for `x`.
* It generates code for the literal `10` (`LLVMConstInt`).
* It generates a `store` instruction to put `10` into the memory of `x`.
* It registers `x` in the codegen symbol table mapping the name to the LLVM value.



---

## 4. Key Technical Decisions

### No `setjmp`/`longjmp`

The codebase intentionally avoids `setjmp`/`longjmp` due to performance overhead ("Those suck ass."). Error recovery is handled via explicit state checks (`p->has_error`) and graceful fallback.

### Internal vs. Public Headers

The parser uses a strict separation of interfaces:

* **`parser.h`**: Contains the AST definitions (`ASTNode`, `VarDeclNode`). This is included by Semantic and Codegen modules.
* **`parser_internal.h`**: Contains function prototypes like `eat()`, `parse_expression()`, and access to the global `current_token`.
* **Why:** This encapsulates the parsing logic. The Semantic analysis module doesn't need to know *how* to parse an expression, it only needs to know what the resulting AST looks like.

### Raw LLVM C API

The project uses the pure C API (`llvm-c/Core.h`) rather than the C++ object-oriented API.

* **Effect:** This requires manual management of strings and builders (`LLVMBuildAdd`, `LLVMBuildRet`).
* **Architecture:** It allows the entire compiler to remain pure C, ensuring high portability and a smaller runtime footprint compared to linking against the C++ LLVM libraries. It requires explicit type creation (e.g., `LLVMInt32Type()`) at every step.

## 5. ALIR (Alkyl Intermediate Representation) & Codegen

Between Semantic Analysis and LLVM Codegen, there is the **ALIR (Alkyl Intermediate Representation)** phase.
* **ALIR Structure**: ALIR flattens the AST into a linear sequence of instructions (`AlirInst`). Each block has `inst_head` and `inst_tail`.
* **Codegen Routing (`src/codegen_llvm/translate/`)**:
  * `core.c`: Contains `translate_inst()` which acts as a massive router for ALIR instructions.
  * `expr.c`: Handles math/logic (`ALIR_OP_ADD`), memory refs, and casts.
  * `flow.c`: Handles jumps, blocks, calls (`ALIR_OP_CALL`), returns.
  * `stmt.c`: Handles memory allocations (`ALIR_OP_ALLOC`) and stores.
  * `misc.c`: Handles compiler-specific/meta operators like `ALIR_OP_SIZEOF` and `ALIR_OP_ALIGNOF`. Note: these *must* be resolved in Codegen rather than Semantic Analysis because LLVM handles target-specific byte constraints and data layout alignments.

### JIT/REPL Macro Handling (`src/driver/cli.c`)
* The `ethyl` REPL acts dynamically, compiling top-level statements via JIT as they are entered.
* **Macro Functions (`meta void ...`)**: When a macro function (e.g. `meta void print(...)`) is parsed, it sets `is_macro = 1` on its `FuncDefNode`.
* **Crucial Rule**: In `cli.c`, we MUST ensure we do not eagerly compile AST nodes into ALIR if `is_macro == 1` (i.e. `if (node->type == NODE_FUNC_DEF && !((FuncDefNode*)node)->is_macro)`). Macros are strictly AST-expansion constructs; eagerly generating ALIR for an unexpanded macro will cause segmentation faults (like `GA ADA COLLECTION!`) since its internal variables (e.g., `for arg in ...`) are unbound placeholders until the call site.

## 6. What to Do
With the given information, do:

<RULE>
Alkyl files use the extensions .kyl (source) and .hky (header) strictly. Do not use or assume .al extensions.
</RULE>

### Unions and ALIR Type Layout
- Unions are registered as synthetic types (`__Union_...`) in the AST and ALIR.
- In `src/semantic/check.c` and `src/semantic/fragment/switch.c`, when processing union field access (either by explicit field name or by type index like `t[int]`), we must first perform an **exact match** on the field type (`sem_types_are_equal`). If an exact match isn't found, we can fall back to a **compatible match** (`sem_types_are_compatible`).
- This two-pass type checking prevents the compiler from mistakenly returning the first implicitly castable field (e.g. `single` when `int` is requested).
- In ALIR `src/codegen_llvm/translate/stmt.c`, `ALIR_OP_GET_PTR` on a union evaluates directly to the base pointer of the union (`op1`), bypassing `LLVMBuildStructGEP2`, because unions share the same start address for all members.

### ALIR Codegen
- Missing instructions in `translate_inst` (like `ALIR_OP_SIZEOF` or `ALIR_OP_ALIGNOF`) will silently assign `NULL` to the destination temporary variable `res`.
- If the next instruction attempts to use this `NULL` temporary variable (e.g., `LLVMTypeOf(op1)`), it will cause a `SIGSEGV` during Codegen phase.
- Ensure all ALIR operators are correctly routed in the giant switch statement in `src/codegen_llvm/translate/core.c` and implemented in their respective files.
- Missing an operator (like `ALIR_OP_GT`) in `translate_inst` will cause it to fall back to `translate_misc`, which may return `NULL`. If this `NULL` is then used in a conditional branch (`ALIR_OP_CONDI`), the conditional branch will silently fail to be emitted. This leaves the current Basic Block without a terminator, causing LLVM to insert an `unreachable` instruction at the end of the block, which crashes at runtime when hit (e.g., inside loops or if-statements).

---

# README Reference
*The following is a complete copy of the project README.md for contextual awareness:*

# Alkyl Programming Language

A simple programming language written using C as a frontend and backends, such as LLVM-IR (mainly), QBE (experimental, but well done), MLIR (very experimental), and Cranelift (mismatch between standard library), and self-backend (to be implemented), with the philosophy of ARCUY: Always Reasonably Choose Your Way.

Alkyl is named after aikil (aiki language: sorry, kind of NPD here as this is a personal project) and alkyl in chemistry which is a highly reactive and foundational. This means alkyl is a very modularized language that can acts as others. The file extension for Alkyl source code is `.kyl`, a sweet suggestion made by my beloved girlfriend (saysay/kupkup), whereas for the header file it is `.hky` because it sounds... *hacky*.

# Purpose

Alkyl is a general multipurpose language that is used for experimenting with lexer, parser, and code generation. Its principle is ARCUY: "Always Reasonably Choose Your Way" which means the keyword "reason" is embedded in its lexer and parser. Moreover, Alkyl is similar to C, Dart, Zig, and some other programming languages.

Alkyl does not try to compete with other languages, even languages like Racket or Lisp. It is used for personal project and to understand how old compilers and modern compilers work. This is why Alkyl is not a revolution in programming language, it is just a side hobby project that may be useful to me and others.

# Pipeline

## Supported Backends
There are some supported backends that is very stable, stable, and undefined behavior. Refer to Flows.

## Flows
Refer to src/ for all of these

### ALIR to LLVM-IR
The mainstream way to do it (currently heavily supported)
lexer (tokenize)
-> parser (to ParseNodes)
-> semantic checker (modified ParseNodes)
-> alir (to alir)
-> alick (checked alir)
-> optlir (optimized alir codes)
-> alick (checked optimized alir)
-> *llvm* (alir to llvm-ir ssa) (results in machine code)

### ALIR to QBE IR (.ssa)
The one we will need to self-host using QBE (run quite slower at its peak, the binary is slower too)
... (still the same up to the last alick)
-> *qbe* (alir to qbe ssa) (results in machine code)

### From Parser to MLIR (.mlir)
This is the least supported and still experimental (just to check if Alkyl is possible to use MLIR)
... (still the same up to semantic checker)
-> *mlir* (to Alkyl's MLIR dialect using .td)
-> llvm (from mlir) (results in machine code)

This is the most **unsupported** as Cranelift's library (Rust architecture) is different than C's. This is why the usage of cranelift is only for experimental, unless the standard library for Alkyl is really implemented, e.g. abstracts the real usage of print, input, .etc without having to hardcode-link to musl/glibc.
... (still the same up to the last alick)
-> *cranelift* (alir to clif) (results in machine code)

# Core Feature

Core feature is the high-level and simplicity of C, combined with C++'s' and Java's object oriented programming with is-a has-a feature.The language itself enforces SOLID principles and modularization, but without as much as bloat in other programming languages.

Alkyl also features a robust, **fully orthogonal** effect and error-handling type system which is heavily similar to Zig:
* **`pure` vs `impure`**: Determines if a function has side-effects (state mutation, IO).
* **`total` vs `partial`**: Determines if a function is guaranteed to terminate. A `total` function must not contain infinite loops or call `partial` functions.
* **`pristine` vs `tainted`**: Determines error-safety. A `tainted` value represents a potential error or failure that must be safely unwrapped (`wash`, `clean`, `untaint`) before it can be used, guaranteeing safety without runtime exceptions.
* **`mutable` vs `immutable`**: Determines if a variable can be modified or not. The default is "mutable" (but marked as immutable for optimization, unless the compiler can prove that it is not immutable)

It also has UFCS (unified function call syntax), e.g. call is treated as if it were a method call. By default, this is true, but can be set to false in the premeta tags.

# Example Code
```c
import "lib/c";

float divby?(float a, float b) {
  if b == 0 then purge ErrFloatDivisionByZero;
  return a / b;
}

int? divby(pristine int a, pristine int b) {
  if (b == 0) {
    purge ErrIntegerDivisionByZero;
  }
  return a / b;
}

meta {
  reason "For testing only"
  lexer.scope_style = SCOPE_INDENTATION;
}

int main() {
  reason "numerator must be an integer"
  immutable int num = 2, denum = 0;
  printf ("%d", divby(1, 0) ? 2);
  // return 2
}
```
# Interactive REPL (Ethyl)

Alkyl comes with `ethyl`, a powerful, highly interactive Read-Eval-Print Loop (REPL). It allows you to write Alkyl code line-by-line, test expressions dynamically, and see immediate feedback without needing a full AOT compilation step. Ethyl leverages the internal `MetaVM` to seamlessly JIT-execute statements, making it excellent for rapid prototyping, debugging, and learning the language.

This "MetaVM" does not depend on JIT like LLVM's JIT, libgccjit, or MIR, but it uses its own method to execute the ALIR Bytecodes. Currently, it supports FFI on Linux-only, hence we can `extern int printf` in the MetaVM itself, thus making the Alkyl's interpreter/REPL (Ethyl) great.

# Building Alkyl
For full instructions on how to build the project using CMake or Bazel, and how to use the different compiler backends (LLVM, QBE, MLIR, Cranelift), please refer to the detailed build guide:
* [alkyl/docs/build.md](docs/build.md)

Quick start with CMake:
```bash
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
./alkyl_llvm ../test/code/general/test_real_faran.kyl --unopt
./out
./out
```

# Testing and Benchmarking
Alkyl comes with two very helpful shell scripts inside `scripts/`:

* **`scripts/run_tests.sh`**: The official test runner. It verifies compilation, execution, and output logging of all test cases in `test/code/`. It runs them in `unopt` and `opt` modes. You can run it via:
  ```bash
  ./scripts/run_tests.sh --parallel
  ```
* **`scripts/compare_bin`**: A benchmarking utility using `hyperfine` (make sure it is installed) and `valgrind`. It compiles a given `.kyl` file across the four compiler backends (LLVM, QBE, MLIR, Cranelift) and compares their compilation time, execution time, binary sizes, and cache misses!
  ```bash
  ./scripts/compare_bin test/code/examples/mandelbrot.kyl --opt --runs 50
  ```

# Docs
For further information, look at
* alkyl/docs/usage: How to use this programming language.
* alkyl/docs/spec: The specification of this language: what can be done, what cannot be done (use MoSCoW analysis here and check whether it is implemented correctly or not).
* alkyl/docs/internal
* alkyl/docs/software-arch: An archictecture containing rigorous definition, formalization, and standardizaton of this project's scope. This includes the Alkyl Programming Language written in C, Alkyl Programming Language written in Alkyl itself, and others.
* alkyl/docs/business-arch: An architecture containing views from business perspective of Alkyl Project.

# BALIR Format
The compiled ALIR module uses a binary representation called `.balir`.
The first magic hex sequence is `fa 8a 11 a1 c1` which stands for faran aiki (because this is really a personal project that I do not plan to very much make it enterprise-like as this is just an experimental)
The second magic hex sequence `2f 58 b0 4f 2e c2 a8 ee 24` is placed at the start of `.balir` files to identify them.
This hex sequence is derived from hashing the word `faranaiki` with SHA256, taking the first 36 characters, decomposing them into 18 hex pairs, and multiplying hex 1 x hex 2 mod 0x100, etc.

# Status

Don't get me wrong, the reason why I use Artificial Intelligence is because I am a programmer, but not a coder. The low-level C project that I never vibecoded is nihwm (my C window manager forked using dwm) and that is not basic setup, but I have to rtfm the X library. Thus, to avoid that painful reading, I gamble and use combined artificial intelligence as the coders.

This is why this project is 60% written by AI, 40% written by myself. Although it is not purely vibecoded as AI cannot generate LLVM and C instructions perfectly, most of the backend and some frontend code is written by Gemini AI, Antigravity, Kilo with Stepfun 3.7, Ling, Hy, and others.

# Why C, not C++, Rust, Zig?

C is the lingua franca of languages. Moreover, C gives you a 100% permission over what you can. This is why Alkyl's basic syntax still looks like C even though many modern programmers use something like `x: int`, but Alkyl still uses `int x` because it is more "computer-like".
Check docs/testing.md for details on testing scripts.

<RULE>
**Line Endings Rule:**
This project uses Linux, not Windows. Always use LF (`\n`) for line endings. Do NOT use CRLF (`\r\n`).
</RULE>

<RULE>
We use Linux, not Windows! Never use CRLF (\r\n) for line endings, always use Unix style LF (\n) only.
</RULE>
