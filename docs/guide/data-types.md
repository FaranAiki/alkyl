# Basic Data Types

Alkyl is a statically-typed language, meaning every variable must have a specific type known at compile time. Alkyl provides several primitive types for your everyday programming needs.

## Variables

You can declare a variable using the `let` keyword. 

```alkyl
let age = 25;           // Type inferred as int
let name: string = "Alkyl"; // Explicitly typed
```

## Primitive Types

Here are the core primitives available in Alkyl:

### Integers
Standard whole numbers.

* **`int`**: 32-bit signed integer. (e.g. `let count = 42;`)
* **`long`**: 64-bit signed integer. (e.g. `let memory = 100000000;`)
* **`char`**: 8-bit integer, usually used for ASCII characters. (e.g. `let letter = 'A';`)

### Floating Point
Numbers with decimals.

* **`single`**: 32-bit floating point (similar to `float` in C). (e.g. `let pi: single = 3.14;`)
* **`double`**: 64-bit floating point for higher precision. (e.g. `let precise_pi = 3.14159265;`)

### Booleans
* **`bool`**: Can be either `true` or `false`.

## Arrays

Alkyl supports clean array syntax.

```alkyl
let numbers = [1, 2, 3, 4, 5];
```

## Type System Notes

Alkyl is strictly typed. It is designed to be completely deterministic about how variables are cast.

```alkyl
// This is perfectly valid
let a = 1;      // int
let b = 2.5;    // double

// Implicit casting to double is preferred for mixed arithmetic!
let c = a + b;  // c is a double
```
