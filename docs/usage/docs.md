# Alkyl Language Specification and Design

> **CRITICAL: Alkyl uses C-like syntax, NOT Rust-like syntax.**
> 
> Function declarations: `int main() { }` (C-style), NOT `main: int`.
> Variable declarations: `int x = 10;` (type before name).
> The `let` keyword is like `auto` in C++ — it infers the type, so `let y: int = 20` is **INVALID**.
> Use `int y = 20;` or `let y = 20;`, never `let y: int = 20;`.
> **`string` is NOT a built-in primitive.** It is a library class in `lib/std/string.kyl` and is NOT fully implemented. Use `char*` for strings.

Alkyl is a statically typed, compiled programming language that blends C-style syntax with modern features like namespaces, classes, generics (templates), and robust error handling (pristine/tainted semantics). It compiles directly to LLVM IR using the pure C `llvm-c` API. It also features a powerful REPL (JIT interpreter) called `ethyl`.

## 1. Basics & Data Types

Alkyl supports standard C-style primitives:
*   `int`, `unsigned int`, `long`, `unsigned long`, `long long`
*   `single` (32-bit float), `double` (64-bit float)
*   `char`, `bool`
*   Arrays and pointers: `int[]`, `int*`, `int[5]`
*   **`string` is NOT a primitive.** It is a library class (`lib/std/string.kyl`) and NOT fully implemented. Use `char*` for strings.

Array literals are fully supported natively:
```alkyl
let arr = [1, 2, 3, 4, 5]; // Infers as int[]
```

## 2. Variables and Assignments

Variables are declared using `let` (type inference is supported):
```alkyl
let x = 10;
int y = 20;
const z = 30;
```
In the interactive REPL (`ethyl`), implicit assignment (e.g., `x = 5;`) acts exactly identically to `let x = 5;`, allowing for seamless Python-like interactions.

## 3. Control Flow

### If and While
Standard C-style conditionals:
```alkyl
if (x > 10) { ... } else { ... }
while (x > 0) { ... }
for i in [1, 2, 3] { ... }
```

### Switch Statements
Alkyl `switch` statements break by default (unlike C). To explicitly fallthrough, use the `leak` keyword:
```alkyl
switch (val) {
    case 1: 
        print("One");
    case 2, 3: 
        print("Two or Three");
    leak case 4: 
        print("Four (leaks to default)");
    default:
        print("Default");
}
```

## 4. Object-Oriented Programming (Classes & Namespaces)

Alkyl supports zero-cost abstraction classes and namespaces:
```alkyl
namespace Math {
    class Vector {
        int x, y;
        
        void set(int x, int y) {
            this.x = x;
            this.y = y;
        }
        
        int[2] get() {
            return [this.x, this.y];
        }
    }
}
```
*   **is-a**: Single inheritance (`class Player is Entity`).
*   **has-a**: Traits/mixins (`class User has Printable`).

### Visibility & Export (C++-style)

Alkyl uses C++-style visibility modifiers for classes, namespaces, functions, and variables:

*   **`public`**: Accessible from any scope.
*   **`private`**: Accessible only within the declaring scope.
*   **`open`**: Can be subclassed or reopened (for namespaces/classes).
*   **`closed`**: Cannot be subclassed or reopened.

Namespaces can be exported for cross-module use:

```alkyl
export namespace Math {
    public class Vector { ... }
    private double epsilon = 0.0001;
}
```

## 5. Generics & Template Argument Deduction

Alkyl supports C++ style templates with **Automatic Argument Deduction**.
```alkyl
define Integer as
  int, unsigned int, long, unsigned long;

compound [type[Integer] Type]
Type rec_gcd(Type a, Type b) {
    if b == 0 then return a;
    return rec_gcd(b, a % b);
}

// The compiler automatically infers Type = int!
let result = rec_gcd(6, 9); 
// Explicit instantiation is also supported: rec_gcd[int](6, 9);
```

## 6. Error Handling (Tainted vs Pristine)

Alkyl uses an innovative type-state system for error handling:
*   **Pristine**: A variable in a valid state.
*   **Tainted**: A potential error state (e.g. division by zero, or the result of a C `extern` function).

If you try to pass a tainted variable to a function expecting a pristine one, the compiler throws an error.
You can unwrap tainted variables safely using `wash`, `untaint`, or the fallback operator `?`.
```alkyl
let a = 10 / 0; // 'a' is marked TAINTED
let safe = a ? 1; // Evaluates to 'a' if pristine, or '1' if error.

wash (a) {
    // a is pristine here
} residue (err) {
    // handle error
}
```

## 7. Macros and Built-ins

*   **Macros**: Defined using `meta void`, macros are expanded at the AST level (not simple text replacement). This is crucial for things like `print(...)`.
*   **Built-ins**: 
    *   `sizeof(type)`: Compile-time size.
    *   `alignof(type)`: Alignment requirements.
    *   `typeof(var)`: Type reflection.
*   **Defer**: `defer { ... }` runs code at the end of the current scope, perfect for manual memory management:
```alkyl
let ptr = malloc(100);
defer { free(ptr); }
```
