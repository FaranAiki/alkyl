## 2026-07-14T21:57:11Z

You are a worker agent for the E2E Testing Track of the Alkyl compiler project.
Your working directory is: /home/faranaiki/Git/alkyl/.agents/worker_e2e_m2

Your task is to execute Milestone 2: Tier 1 Tests (Feature Coverage).
Please implement 20 test cases (5 for each of the 4 features) under the `test/code/`, `test/input/`, and `test/output/` directories.

The 4 features are:
1. Lexer Settings (`double_quote_as_string` defaulting to true)
2. Default / Optional Arguments
3. Named Arguments
4. Advanced Object Instantiation & Constructors

For each test case, you must create:
- The `.aky` file in `test/code/<feature>/<name>.aky`
- An empty (or relevant input) `.in` file in `test/input/<feature>/<name>.in`
- An expected stdout/stderr `.out` file in `test/output/<feature>/<name>.out` (what the program SHOULD output when compiled and run correctly)
- If the test is a negative test (e.g. invalid syntax that should fail compilation), you must also create the `.log` expected compilation error log in `test/log/<feature>/<name>.log`

Here is the list of 20 test cases to implement:

### Feature 1: Lexer Settings (directory: `lexer_settings`)
1. `f1_default_enabled`: Double-quoted strings default to active `string(c"...")`.
   - `.aky`: Prints the `typeof` a double-quoted string. It should be "string".
     ```alkyl
     extern int printf(c-string fmt, ...);
     int main() {
         let s = "hello";
         printf(c"%s\n", typeof(s));
         return 0;
     }
     ```
   - `.out`: Contains `string`.
2. `f1_explicit_disabled`: Disabling setting via `// FLAGS: --no-double-quote-as-string`.
   - `.aky`: Has `// FLAGS: --no-double-quote-as-string` as the first line. Prints the `typeof` a double-quoted string. It should be C-string type.
     ```alkyl
     // FLAGS: --no-double-quote-as-string
     extern int printf(c-string fmt, ...);
     int main() {
         let s = "hello";
         printf(c"%s\n", typeof(s));
         return 0;
     }
     ```
     (Note: check if C-string type name is `char*` or `c-string` or `ptr(char)` by inspecting types in `test/general/string.aky` or type analyzer.)
   - `.out`: Contains the type name for C-strings.
3. `f1_cstring_unaffected`: Explicit `c"..."` is parsed as C-string under both settings.
   - `.aky`: Prints `typeof(c"hello")`.
     ```alkyl
     extern int printf(c-string fmt, ...);
     int main() {
         let s = c"hello";
         printf(c"%s\n", typeof(s));
         return 0;
     }
     ```
   - `.out`: Contains C-string type name.
4. `f1_setting_explicit_enabled`: Explicitly enabled setting using `// FLAGS: --double-quote-as-string`.
   - `.aky`: Has `// FLAGS: --double-quote-as-string` as the first line. Prints `typeof("hello")`.
   - `.out`: Contains `string`.
5. `f1_escape_handling`: Escaped characters in string literals are handled correctly under `double_quote_as_string`.
   - `.aky`: Prints a string with escapes, e.g. `"line1\nline2"`.
   - `.out`: Contains:
     ```
     line1
     line2
     ```

### Feature 2: Default Arguments (directory: `default_args`)
6. `f2_single_default`: Function with one default argument at the end.
   - `.aky`: `int add(int a, int b = 5) { return a + b; }` and call `add(10)`. Print result.
   - `.out`: `15`.
7. `f2_multi_default`: Function with multiple default arguments.
   - `.aky`: `int sum(int a, int b = 2, int c = 3) { return a + b + c; }` and call `sum(1)`. Print result.
   - `.out`: `6`.
8. `f2_class_method_default`: Class method with default arguments.
   - `.aky`: Class `Calculator` with method `multiply(int a, int b = 2)` and call it. Print result.
   - `.out`: Expected multiplication result.
9. `f2_override_default`: Calling with explicit argument to override default.
   - `.aky`: Call `add(10, 20)` (from case 6). Print result.
   - `.out`: `30`.
10. `f2_all_default`: Function where all arguments have default values.
    - `.aky`: `int zero(int a = 0, int b = 0) { return a + b; }` and call `zero()`. Print result.
    - `.out`: `0`.

### Feature 3: Named Arguments (directory: `named_args`)
11. `f3_named_only`: Calling with named arguments in matching declaration order.
    - `.aky`: `int sub(int a, int b) { return a - b; }` called as `sub(a = 10, b = 4)`. Print result.
    - `.out`: `6`.
12. `f3_named_shuffled`: Calling with named arguments in non-declaration order.
    - `.aky`: Call `sub(b = 4, a = 10)`. Print result.
    - `.out`: `6`.
13. `f3_mixed_pos_named`: Positional arguments first, then named.
    - `.aky`: Call `sub(10, b = 4)`. Print result.
    - `.out`: `6`.
14. `f3_method_named`: Calling class methods with named arguments.
    - `.aky`: Class `Calculator` with method `divide(int a, int b)` called as `divide(b = 2, a = 10)`. Print result.
    - `.out`: `5`.
15. `f3_named_with_defaults`: Calling a function that has default arguments, passing some as named and omitting some.
    - `.aky`: `int val(int a, int b = 1, int c = 2) { return a * b * c; }` called as `val(a = 5, c = 3)`. Print result.
    - `.out`: `15`.

### Feature 4: Advanced Object Instantiation & Constructors (directory: `constructors`)
16. `f4_implicit_init`: Implicit struct-like instantiation using attributes when no `init` is defined.
    - `.aky`: Class `Point { int x; int y; }` instantiated as `let p = Point(x = 10, y = 20)`. Print `p.x` and `p.y`.
    - `.out`:
      ```
      10
      20
      ```
17. `f4_custom_init`: Custom constructor defined as `init(...) { ... }`.
    - `.aky`: Class `Person` with attribute `int age;` and `init(int a) { age = a; }`. Instantiated as `let p = Person(25)`. Print `p.age`.
    - `.out`: `25`.
18. `f4_void_init`: Custom constructor defined as `void init(...) { ... }`.
    - `.aky`: Class `Person` with `void init(int a) { age = a; }`. Instantiated as `let p = Person(30)`. Print `p.age`.
    - `.out`: `30`.
19. `f4_cstyle_init`: Custom constructor defined as C-style `ClassName(...) { ... }`.
    - `.aky`: Class `Person` with `Person(int a) { age = a; }`. Instantiated as `let p = Person(35)`. Print `p.age`.
    - `.out`: `35`.
20. `f4_overloaded_init`: Class with multiple custom constructors.
    - `.aky`: Class `Person` with `init(int a) { age = a; }` and `init() { age = 18; }`. Instantiated both ways. Print both ages.
    - `.out`:
      ```
      25
      18
      ```

Please write these 20 tests. Make sure you also create the corresponding input `.in` files (they can be empty files).
Write a handoff report when complete.

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A Forensic Auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.
