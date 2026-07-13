# Alkyl Programming Language

A simple programming language written using LLVM-IR.

Alkyl is named after aikil (aiki language) and alkyl in chemistry which is a highly reactive and foundational.

# Status

60% written by AI, 40% written by myself. Although it is not purely vibecoded as AI cannot generate LLVM and C instructions perfectly, most of the backend and some frontend code is written by Gemini AI.

# Purpose

Alkyl is a general multipurpose language that is used for experimenting with lexer, parser, and code generation.

Moreover, Alkyl used syntax similar to C "but lazy" so developer can understand had there been a bug that AI missed.

# Core Feature

Core feature is the high-level and simplicity of C, combined with C++'s object oriented programming with is-a has-a feature.

Alkyl also features a robust, **fully orthogonal** effect and error-handling type system:
* **`pure` vs `impure`**: Determines if a function has side-effects (state mutation, IO).
* **`pristine` vs `tainted`**: Determines error-safety. A `tainted` value represents a potential error or failure that must be safely unwrapped (`wash`, `clean`, `untaint`) before it can be used, guaranteeing safety without runtime exceptions.

Todo implemented:
* threading { }
* inert so that inert entities cannot be has-ed
* crazy docs
* crazy error handling

# Docs
For further information, look at
* alkyl/docs/usage: How to use this programming language.
* alkyl/docs/spec: The specification of this language: what can be done, what cannot be done (use MoSCoW analysis here and check whether it is implemented correctly or not).
* alkyl/docs/internal
* alkyl/docs/software-arch: An archictecture containing rigorous definition, formalization, and standardizaton of this project's scope. This includes the Alkyl Programming Language written in C, Alkyl Programming Language written in Alkyl itself, and others.
* alkyl/docs/business-arch: An architecture containing views from business perspective of Alkyl Project.
