---
name: alkyl-language
description: Understand and write code in the Alkyl programming language, including syntax, types, error handling (pristine/tainted), and standard library usage.
---

# Alkyl Language Guide

Alkyl is a statically typed, compiled programming language that blends C-style syntax with modern features like namespaces, classes (with traits), string handling, and macro definitions. It compiles to LLVM IR.

## 1. Basics

### Comments
- `// Single-line comment`
- `/* Multi-line comment */`

### Data Types
- `int`: 32-bit Integer
- `char`: 8-bit Character
- `bool`: Boolean (`true`/`false`)
- `single`: Single-precision floating point (float)
- `double`: Double-precision floating point
- `string`: String type (managed `char*`)
- `void`: Empty type
- `let` / `auto`: Type inference

### Variables
- Explicit: `int x = 10;`
- Inferred: `let y = 20.5;`
- Mutable/Immutable: `mut int count = 0;` vs `imut int id = 123;`
- Pointers: `int* ptr = null;`

### Arrays
- `int numbers[5];`
- `int arr[] = [1, 2, 3];`

## 2. Control Flow
- **If-Else**: `if x > 10 { ... } elif (x == 10) { ... } else { ... }`
- **While**: `while x > 0 { x--; }`
- **Do-While**: `while once x > 0 { x--; }`
- **Loop**: `loop 5 { ... }`

## 3. Functions
```alkyl
int add(int a, int b) { return a + b; }
```
Supports function overloading.

## 4. Object-Oriented Programming
- **Classes**: `open class Vector { int x; int y; void print_vec() { ... } }`
- **Inheritance**: `class Player is Entity { ... }`
- **Traits (Mixins)**: `class User has Printable { ... }`
- **Visibility**: `open` (public), `closed` (private).

## 5. Metaprogramming & Macros
- `define PI as 3.14159;`
- `define MAX(a, b) as ((a > b) ? a : b);`
- `define foo, bar as 100;`
- `typedef Integer as int;`

## 6. Namespaces & Enums
- **Namespaces**: `namespace Math { ... }`
- **Enums**: `enum Color { Red, Green = 10, Blue }`

## 7. Modular Programming & FFI
- **Import**: `import "std/math.aky";`
- **Extern**: `extern int printf(string fmt, ...);`
- **Link**: `link "m";`

## 8. Switch Statement
Supports `switch` with explicit fallthrough (`leak`).
```alkyl
switch (val) {
    case 1: print("One");
    leak case 4: print("Four (leaks to default)");
    default: print("Default");
}
```
Or unleaked by default. You can use `leak switch` and `unleak case`.

## 9. Error Handling (Pristine vs Tainted)
- **pure/impure**: Functions are pure by default. Extern functions are ALWAYS impure. `pure` can be explicit.
- **pristine/tainted**: Orthogonal to pure/impure. Extern functions are tainted by default.
- If a pristine function takes a pristine argument `t`, passing a tainted variable causes a semantic error.
- **Fallback operator**: `f(t ? 1)` - if `t` is tainted, use `1`.
- **untaint**: `untaint (expr) residue (err) { ... }` - if `expr` is error, execute residue block. Otherwise, it is pristine.
- **wash / clean**: `wash (expr) { ... } residue (err) { ... }` converts tainted variables to pristine within the main block and handles errors in the residue block.

## 10. Built-ins
- `typeof(expr)`: Gets Type info struct at runtime.
- `sizeof(type/var)`: Size in bytes.
- `alignof(type/var)`: Alignment in bytes.
- `defer { ... }`: Executes block before exiting scope.
- `purge ErrName`: Runtime error throw. Handled globally.
- `<<%` (left rotate), `%>>` (right rotate), `^` (XOR).
- `/`: Division automatically checks for zero and emits `purge ErrDivisionByZero` at runtime, marks result as tainted if denominator is a variable.

## 11. Architecture
- **Lexer**: Tokenization.
- **Parser**: Recursive Descent (AST Construction), uses `setjmp`/`longjmp` for error recovery.
- **Semantic Analysis**: Type checking, symbol resolution, overload resolution, name mangling.
- **CodeGen**: Generates LLVM IR via pure C API.
- **JIT**: REPL uses LLVM JIT.
- **AOT**: Compiles to object code (`out.o`) and links via `gcc`.
