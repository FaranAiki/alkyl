# Reflection Engine & Standard Library Update

The basic reflection engine and standard library types have been implemented!

## Test Cases Created
- `test/string/test_string.aky`
- `test/vector/test_vector.aky`
- `test/hashmap/test_hashmap.aky`
- `test/set/test_set.aky`
- `test/has/test_has.aky`

## Current Status
The compiler successfully runs up to the **LLVM Codegen** phase for `test_has.aky`.

**Known Issue (Codegen):**
Because we replaced the hardcoded `string` with a `class string`, string literals like `"hello"` are now generating an `i8*` pointer in LLVM, but the AST expects a struct `{ i32 len; i8* data; }`.
This causes a type mismatch: `SExt only operates on integer`.

**Next Step for Compiler:**
In `src/llvm_codegen/codegen.c`, when encountering an `ALIR_VAL_STRING` whose expected type is `TYPE_CLASS("string")`, we need to emit LLVM instructions to allocate the `{ i32, i8* }` struct, store the length, and store the pointer.
