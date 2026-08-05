# Alkyl Intermediate Representation (ALIR) Text Format Specification

The Alkyl Intermediate Representation (ALIR) uses a specific text formatting structure for human-readable debugging output (`.raw.alir` and `.opt.alir`). This document specifies the syntax and semantics for reading the textual ALIR format.

## Overview

A typical ALIR file contains:
- Enums, Structs, and Global definitions
- Function definitions and declarations
- Basic blocks containing linear instructions

## Values and Identifiers

- **Temporaries**: Prefixed with `%t` followed by a number (e.g., `%t0`, `%t1`). These act as local virtual registers.
- **Variables**: Prefixed with `%` followed by the variable name (e.g., `%x`, `%y`). 
- **Globals / Functions**: Prefixed with `@` (e.g., `@main`, `@printf`).
- **Constants**: Written as literal values (e.g., `5`, `3.14`).

## Types

Every operation and allocation strictly annotates its related type wrapped in brackets `[type]`. For example:
- Primitive types: `[int]`, `[float]`, `[bool]`, `[double]`, `[void]`
- Classes / User types: `[%String]`
- Pointers: Asterisks denote pointers inside the brackets, e.g., `[int*]`.

## Instruction Syntax

### 1. Operations (Standard Assignments)

General operations take the format:
```
<destination> = <opcode>[<type>] <operand1>, <operand2> (optional arguments)
```
*Example:*
```
%t2 = add[int] %t1, 2
%t6 = eq[bool] %t4, 7
%t5 = not[bool] %t6
```

### 2. Memory Allocation (`alloc`)

Stack allocations declare memory for a given type and yield a pointer.
```
<destination> = alloc[<type>]
```
*Example:*
```
%t0 = alloc[int]
```

### 3. Memory Write (`store`)

Storing a value into a memory address uses the `<-` operator, with the pointer on the left and the value on the right, decorated by the type of the value being stored.
```
<destination_ptr> <-[<type>] <value>
```
*Example:*
```
%t0 <-[int] 5
```
*(Here `%t0` is the pointer, and `5` is the `int` value being stored.)*

### 4. Memory Read (`load`)

Loading from memory follows the standard assignment syntax.
```
<destination> = load[<type>] <source_ptr>
```
*Example:*
```
%t1 = load[int] %t0
```

### 5. Return Statements (`ret`)

Return statements use the `->` operator.
```
-> [<type>] <value>
```
*Example:*
```
-> [int] 1
-> [void]
```

### 6. Control Flow (`condi`, `jump`)

Conditional jumps evaluate a boolean condition.
```
if <condition> then <target_block_label> <fallback_block_label>
```

Unconditional jumps:
```
jump <target_block_label>
```

### Example Breakdown

```c
// C-like source:
// int x = 5; 
// return x + 2;

// ALIR Output:
%t0 = alloc[int]           // Allocate memory for an integer (pointer stored in %t0)
%t0 <-[int] 5              // Store value 5 into pointer %t0
%t1 = load[int] %t0        // Load the value from %t0 into temporary %t1
%t2 = add[int] %t1, 2      // Add 2 to %t1 and store result in %t2
-> [int] %t2               // Return %t2
```
