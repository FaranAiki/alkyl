# How to compare strings

Instead of comparing like `a == b` (which is invalid in C) or `!strcmp(a, b)` which is ok, but not good enough, we use string interning and a "function" called `streq_lit` (which is string equal literal).
