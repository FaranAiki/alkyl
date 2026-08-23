# Alkyl Programming Language

A simple programming language written using C as a frontend and backends, such as LLVM-IR (mainly), QBE (experimental, but well done), MLIR (very experimental), and Cranelift (mismatch between standard library), and self-backend (to be implemented), with the philosophy of ARCUY: Always Reasonably Choose Your Way.

Alkyl is named after aikil (aiki language: sorry, kind of NPD here as this is a personal project) and alkyl in chemistry which is a highly reactive and foundational. This means alkyl is a very modularized language that can acts as others. The file extension for Alkyl source code is `.kyl`, a sweet suggestion made by my beloved girlfriend (saysay/kupkup), whereas for the header file it is `.hky` because it sounds... *hacky*.

# Purpose

Alkyl is a general multipurpose language that is used for experimenting with lexer, parser, and code generation. Its principle is ARCUY: "Always Reasonably Choose Your Way" which means the keyword "reason" is embedded in its lexer and parser. Moreover, Alkyl is similar to C, Dart, Zig, and some other programming languages.

Alkyl does not try to compete with other languages, even languages like Racket or Lisp. It is used for personal project and to understand how old compilers and modern compilers work (duh, mostly vibe coded so I didn't 101% rtfm). This is why Alkyl is not a revolution in programming language, it is just a side hobby project that may be useful to me and others (99% not useful).

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

Alkyl also features C++-style visibility modifiers: `public`, `private`, `open`, `closed`. Classes and namespaces can use these modifiers. Namespaces can be exported with `export namespace`.

**Note on `string`**: Alkyl does NOT have a built-in `string` primitive. The `string` type is a library class defined in `lib/std/string.kyl` and is NOT fully implemented (contains TODOs). Use `char*` for strings unless explicitly importing the `string` library.

Alkyl also features a robust, **fully orthogonal** effect and error-handling type system which is heavily similar to Zig:
* **`pure` vs `impure`**: Determines if a function has side-effects (state mutation, IO).
* **`total` vs `partial`**: Determines if a function is guaranteed to terminate. A `total` function must not contain infinite loops or call `partial` functions (not 100% implemented).
* **`pristine` vs `tainted`**: Determines error-safety. A `tainted` value represents a potential error or failure that must be safely unwrapped (`wash`, `clean`, `untaint`) before it can be used, guaranteeing safety without runtime exceptions. There are some bugs in here.
* **`mutable` vs `immutable`**: Determines if a variable can be modified or not. The default is "mutable" (but marked as immutable for optimization, unless the compiler can prove that it is not immutable)

It also has UFCS (unified function call syntax), e.g. call is treated as if it were a method call. By default, this is true, but can be set to false in the premeta tags.

# Example Code
```c
import "std/print";

define float as single;

errnum[ErrFloatDivisionByZero]
float divby?(float a, float b) {
  if b == 0 then purge ErrFloatDivisionByZero;
  return a / b;
}

errnum[ErrIntegerDivisionByZero]
int? divby(pristine int a, pristine int b) {
  if (b == 0) {
    purge ErrIntegerDivisionByZero;
  }
  return a / b;
}

premeta {
  reason "For testing only"
  lexer.scope_style = SCOPE_INDENTATION;
}

int main() {
  reason "numerator must be an integer"
  immutable int num = 2, denum = 0;
  std.printf ("{}", divby(1, 0) ? 2);
  // prints 2 because 1 / 0 is an error
  return 0;
}
```
# Interactive REPL (Ethyl)

Alkyl comes with `ethyl`, a powerful, highly interactive Read-Eval-Print Loop (REPL). It allows you to write Alkyl code line-by-line, test expressions dynamically, and see immediate feedback without needing a full AOT compilation step. Ethyl leverages the internal `MetaVM` to seamlessly JIT-execute statements, making it excellent for rapid prototyping, debugging, and learning the language. (I hope this is better than Python/Julia for some reasons lol prob not)

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
```

# Testing and Benchmarking
Alkyl comes with two very helpful shell scripts inside `scripts/`:

* **`scripts/run_tests.sh`**: The official test runner. It verifies compilation, execution, and output logging of all test cases in `test/code/`. It runs them in `unopt` and `opt` modes. You can run it via:
  ```bash
  ./scripts/run_tests.sh
  ```
* What I often use is
```bash
  ./scripts/run_single.sh --update --llvm
```
*  and for QBE, I use
```bash
  ./scripts/run_single.sh --update --qbe
```

* **`scripts/compare_bin`**: A benchmarking utility using `hyperfine` (make sure it is installed) and `valgrind`. It compiles a given `.kyl` file across the four compiler backends (LLVM, QBE, MLIR, Cranelift) and compares their compilation time, execution time, binary sizes, and cache misses!
  ```bash
  ./scripts/compare_bin test/code/examples/fizzbuzz.kyl --opt --runs 50
  ```

# Docs
For further information, look at
* `alkyl/docs/usage`: How to use this programming language.
* `alkyl/docs/spec`: The specification of this language: what can be done, what cannot be done (use MoSCoW analysis here and check whether it is implemented correctly or not).
* `alkyl/docs/internal`
* `alkyl/docs/software-arch`: An archictecture containing rigorous definition, formalization, and standardizaton of this project's scope. This includes the Alkyl Programming Language written in C, Alkyl Programming Language written in Alkyl itself, and others. (somewhat unused)
* `alkyl/docs/business-arch`: An architecture containing views from business perspective of Alkyl Project. (somewhat unused)

# BALIR Format
The compiled ALIR module uses a binary representation called `.balir`.
The first magic hex sequence is `fa 8a 11 a1 c1` which stands for faran aiki (because this is really a personal project that I do not plan to very much make it enterprise-like as this is just an experimental)
The second magic hex sequence `2f 58 b0 4f 2e c2 a8 ee 24` is placed at the start of `.balir` files to identify them.
This hex sequence is derived from hashing the word `faranaiki` with SHA256, taking the first 36 characters, decomposing them into 18 hex pairs, and multiplying hex 1 x hex 2 mod 0x100, etc.

# Status

Don't get me wrong, the reason why I use Artificial Intelligence is because I am a programmer, but not a coder. Just kidding, I suck at programming and I don't have any talent in software engineering.

On the other side, the low-level C project that I never vibecoded is nihwm (my C window manager forked using dwm) and that is not basic setup, but I have to rtfm the X library. Thus, to avoid that painful reading, I gamble and use combined artificial intelligence as the coders.

This is why this project is 60% written by AI, 40% written by myself. Although it is not purely vibecoded as AI cannot generate LLVM and C instructions perfectly, most of the backend and some frontend code is written by Gemini AI, Antigravity, Kilo with Stepfun 3.7, Ling, Hy, and others.

# Why Alkyl looks like C, not C++, Rust, Zig?

C is the lingua franca of languages. Moreover, C gives you a 100% permission over what you can. This is why Alkyl's basic syntax still looks like C even though many modern programmers use something like `x: int`, but Alkyl still uses `int x` because it is more "computer-like".

# Why Use C?

As I said, I am not a software engineer nor a programmer, so the language that I am really fond of is the high level programming language C.

Check docs/testing.md for details on testing scripts.
