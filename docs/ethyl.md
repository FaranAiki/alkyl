# Ethyl REPL User Guide

Welcome to **Ethyl**, Alkyl's highly interactive Read-Eval-Print Loop (REPL). Ethyl allows you to write Alkyl code line-by-line, test expressions dynamically, and get immediate feedback without having to compile your entire project.

## Starting Ethyl

To start the REPL, simply run the compiled `ethyl` binary from the build directory:

```bash
./build/ethyl
```

You should see a prompt like this:
```
Ethyl (Alkyl interpreter) by Faran Aiki 
Type 'exit' or 'quit' to leave.

In [0]: 
```

## Basic Features

### 1. Evaluating Expressions
You can evaluate basic expressions directly. The REPL dynamically compiles the expression using the `MetaVM` and outputs the result immediately.

```c
In [0]: 1 + 2
3
```

**The `res` variable:**
Ethyl automatically tracks the result of your last evaluated expression and saves it in a global variable named `res`. This is incredibly useful for chained calculations.

```c
In [1]: res * 10
30
```

### 2. Variable Declarations
Variables can be declared dynamically. In Ethyl, you can either use the explicit `let` syntax or just declare them implicitly.

```c
In [2]: let name = "Alkyl"
In [3]: version = 1.0
```

### 3. Function Definitions
You can define full functions in the REPL. Ethyl seamlessly parses them and makes them available for subsequent calls.

```c
In [4]: int add(int a, int b) { return a + b; }
In [5]: add(res, 5)
35
```

### 4. Object-Oriented Programming (OOP)
You can define classes, allocate objects, and call methods, just like in normal Alkyl code.

```c
In [6]: class Point { int x; int y; int sum() { return this.x + this.y; } }
In [7]: let p = new Point { x = 10, y = 20 }
In [8]: p.sum()
30
```

### 5. Error Recovery
Ethyl uses robust internal recovery mechanisms (`setjmp`/`longjmp`). If you make a syntax error, a semantic type error, or even a runtime panic (like a `purge` event), Ethyl will gracefully catch it, print an informative error with a hint, and return you to the prompt without crashing!

```c
In [9]: 1 / 0
purge: ErrDivisionByZero
In [10]: unknown_function()
error: Undefined function or class 'unknown_function'
```

### 6. Standard Library
Ethyl automatically loads standard library conveniences (like `std/ethyl`, `std/print`, etc.) upon startup, meaning functions like `printf` are available by default.

## Exiting
To exit the REPL, simply type `exit` or `quit`, or press `Ctrl+C`.

Enjoy prototyping with Ethyl!
