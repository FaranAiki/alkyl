# Modifiers in Alkyl

Alkyl provides several modifiers to enforce constraints on functions and data. Modifiers enhance the compiler's ability to reason about your code and ensure safety.

## `total` and `partial`

In Alkyl, functions are analyzed for their termination guarantees:

- **`total`**: A total function is guaranteed to terminate and return a result. It cannot contain infinite loops or indeterminate looping constructs (like `while` loops). Furthermore, a `total` function cannot call a `partial` function.
- **`partial`**: A partial function is not guaranteed to terminate (it may loop forever or panic).

**Default Behavior:**
By default, all functions are assumed to be `total` *unless* they contain a `while` loop or call another `partial` function. If a function contains a `while` loop, it is automatically marked as `partial` by the compiler, but this **does not** cause a compilation error.

**Explicit Marking:**
You can explicitly mark a function using the `total` or `partial` keywords:

```c
// Valid: explicitly marking as partial
partial int calculate_forever() {
    while (true) {
        // do something
    }
}

// Error: A total function cannot call a partial function
total int do_work() {
    return calculate_forever(); // COMPILER ERROR
}

// Error: A total function cannot contain a while loop
total int loop_work() {
    while (1) { // COMPILER ERROR
    }
}
```

These modifiers behave similarly to totality checking in languages like Idris or Agda.

## `pristine` and `tainted`

- **`pristine`**: A pristine function is guaranteed not to throw or return unhandled errors.
- **`tainted`**: A tainted function may throw or return an error, and its result must be explicitly unwrapped or handled.

## `pure` and `impure`

- **`pure`**: A pure function does not cause side effects (like mutating global state or performing I/O).
- **`impure`**: An impure function may have side effects.
