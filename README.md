# Alkyl Programming Language

A simple programming language written using C and LLVM-IR.

Alkyl is named after aikil (aiki language) and alkyl in chemistry which is a highly reactive and foundational.

# Status

60% written by AI, 40% written by myself. Although it is not purely vibecoded as AI cannot generate LLVM and C instructions perfectly, most of the backend and some frontend code is written by Gemini AI & Antigravity.

# Purpose

Alkyl is a general multipurpose language that is used for experimenting with lexer, parser, and code generation. Its principle is ARCUY: "Always Reasonable Choose Your Way" which means the keyword "reason" is embedded in its lexer and parser.

Moreover, Alkyl is similar to C, Dart, Zig, and some other programming languages.

# Core Feature

Core feature is the high-level and simplicity of C, combined with C++'s object oriented programming with is-a has-a feature.The language itself enforces SOLID principles and modularization, but without as much as bloat in other programming languages.

Alkyl also features a robust, **fully orthogonal** effect and error-handling type system which is heavily similar to Zig:
* **`pure` vs `impure`**: Determines if a function has side-effects (state mutation, IO).
* **`pristine` vs `tainted`**: Determines error-safety. A `tainted` value represents a potential error or failure that must be safely unwrapped (`wash`, `clean`, `untaint`) before it can be used, guaranteeing safety without runtime exceptions.

# Example Code
```alkyl
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
  int num = 2, denum = 0;
  printf ("%d", divby(1, 0) ? 2);
  // return 2
}
```

# Docs
For further information, look at
* alkyl/docs/usage: How to use this programming language.
* alkyl/docs/spec: The specification of this language: what can be done, what cannot be done (use MoSCoW analysis here and check whether it is implemented correctly or not).
* alkyl/docs/internal
* alkyl/docs/software-arch: An archictecture containing rigorous definition, formalization, and standardizaton of this project's scope. This includes the Alkyl Programming Language written in C, Alkyl Programming Language written in Alkyl itself, and others.
* alkyl/docs/business-arch: An architecture containing views from business perspective of Alkyl Project.

# BALR Format
The compiled ALIR module uses a binary representation called `.balir`.
The magic hex sequence `2f 58 b0 4f 2e c2 a8 ee 24` is placed at the start of `.balir` files to identify them.
This hex sequence is derived from hashing the word `faranaiki` with SHA256, taking the first 36 characters, decomposing them into 18 hex pairs, and multiplying hex 1 x hex 2 mod 0x100, etc.
