# Hello World

Welcome to Alkyl! Let's start by writing the traditional "Hello World" program.

Alkyl makes writing simple programs completely frictionless. 

## The Simplest Program

In Alkyl, you don't even need a `main` function for simple scripts if you are using the interactive REPL (`ethyl`). However, for a fully compiled program, here is how you do it:

```alkyl
import "std/print";

public int main() {
    print("Hello, World!");
    return 0;
}
```

### Breakdown
1. **`import "std/print";`**: This includes the standard print library so we can output text to the console.
2. **`public int main()`**: This is the entry point of our compiled program. It returns an integer status code (0 for success).
3. **`print(...)`**: A macro function from the standard library that prints the given string to the standard output.

## Running It

### 1. Using the Compiler (AOT)
If you want to compile this into a standalone native executable:
```bash
# Compile it
alkyl build main.kyl

# Run the executable
./main
```

### 2. Using the REPL (JIT)
If you just want to run code interactively, open `ethyl` (the Alkyl REPL):
```bash
ethyl
```
Then simply type:
```alkyl
import "std/print";
print("Hello, World!");
```
And watch it execute instantly!
