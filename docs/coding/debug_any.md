# How to Debug Properly

Don't use `fprintf` or `printf`, but `debug_type` where type is the current pipeline process.

For instance, if the file is in the `lexer`, then we use `debug_lexer`, or if the file is currently in `codegen`, then we use `debug_codegen`.

This macro is defined in the `include/common/debug.h` which is just equivalent to `debug_any`

# Why?

So we can do something like `cmake --DCMAKE_BUILD_TYPE=Release` and this is *very clean* instead of manually removing `printf`

