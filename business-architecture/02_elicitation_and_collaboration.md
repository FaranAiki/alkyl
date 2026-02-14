# Alkyl as Business for Elicitation and Collaboration

(wrong pasting)

# 1. Introduction

This knowledge area describes how requirements for the Alkyl language are drawn out (elicited) from stakeholders and research sources, and how ongoing collaboration ensures those requirements are correctly understood.

# 2. Prepare for Elicitation

## 2.1 Research Sources

Comparative Language Analysis: analyzing features in C, C++, Rust, Zig, and Odin to determine "best-in-class" syntax and semantics.

LLVM Documentation: Reviewing llvm-c headers to understand the capabilities and limitations of the backend.

System Programming Standards: Reviewing ABI specifications (System V ABI) to ensure interoperability requirements are met.

## 3 Conduct Elicitation

### 3.1. Elicitation Techniques

Document Analysis: Studying existing codebases (libc headers) to determine what features Alkyl needs to seamlessly import/bind to them.

Prototyping (Spikes): Creating small, throwaway parsers or codegen tests (e.g., code/test/) to verify if a syntax idea is parsable and implementable.

Interface Analysis: Defining the interface between Alkyl and the C runtime (CRT).

## 4 Confirm Results

### 4.1 Validation

Syntax Check: verifying that the grammar (defined in misc/tree-sitter/grammar.js) is unambiguous.

Peer Review: Contributors review Pull Requests to ensure the implementation matches the elicited requirement.

## 5. Communicate Business Analysis Information

### 5.1. Communication Channels

GitHub Issues: The primary source of truth for "To-Do" items and bug reports.

Code Comments: Inline documentation (// TODO: ...) serves as immediate context for requirements within the source files.

Architecture Diagrams: Flowcharts explaining the compilation pipeline (Lexer -> Parser -> Semantic -> Codegen).
