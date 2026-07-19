# Error Handling Overhaul

## 1. Lexer Changes
Add tokens for keywords: `errnum`, `purge`, `clean`, `wash`, `tail`, `tainted`, `pristine`.
Add token for `??` (fallback) and `?` (error query).

## 2. Parser Changes
- `errnum [a, b, c]`: Parsed as a top-level error enumeration (ErrNumDeclNode).
- `purge [Err]`: Parsed as a control-flow statement (PurgeStmtNode).
- Expressions: Parse `x ?? fallback` and `x ? [Err] fallback`.
- Type modifiers: `tainted int` and `pristine int`.

## 3. Semantic Analysis
- `errnum` elements populate the symbol table starting at 1 (0 = NoError).
- Enforce that `main()` must evaluate to a `pristine` context.
- Functions containing `purge` are implicitly `tainted`.

## 4. Codegen
- Convert `tainted T` to an LLVM struct equivalent to `{ T value, i32 error_code }`.
