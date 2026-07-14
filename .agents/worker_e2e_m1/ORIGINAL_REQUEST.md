## 2026-07-14T14:44:58Z

You are a worker agent for the E2E Testing Track of the Alkyl compiler project.
Your working directory is: /home/faranaiki/Git/alkyl/.agents/worker_e2e_m1

Your task is to execute Milestone 1: Test Harness & Infra.
Please perform the following steps:
1. Initialize your progress.md in your working directory.
2. Ensure the directories exist:
   - test/code/sanity
   - test/input/sanity
   - test/output/sanity
   - test/log/sanity
3. Inspect and modify `scripts/run_tests.sh` to:
   - Support custom compilation flags per test case. Specifically, if the first line of the `.aky` file starts with `// FLAGS: `, extract the flags and pass them when running `./alkyl`. E.g., if it has `// FLAGS: --no-double-quote-as-string`, then run `./alkyl --no-double-quote-as-string "$AKY_FILE"`.
   - Correctly support negative tests: if compilation fails (non-zero exit code and no `./out` produced), and an expected compilation log file `test/log/$FEATURE/$NAME.log` exists, the test should PASS if the actual compilation output diffs successfully against the expected log. If the exit code is non-zero but no expected log exists, or if the diff fails, the test should FAIL.
4. Create a positive sanity test:
   - File: `test/code/sanity/hello.aky`
     Contents:
     ```alkyl
     int main() {
         print "%s\n", c"Hello World!";
         return 0;
     }
     ```
   - File: `test/input/sanity/hello.in` (empty file)
   - File: `test/output/sanity/hello.out`
     Contents:
     ```
     Hello World!
     ```
5. Create a negative sanity test:
   - File: `test/code/sanity/syntax_err.aky`
     Contents:
     ```alkyl
     int main() {
         let x = 10
         return 0;
     }
     ```
     (Note the missing semicolon after 10)
   - File: `test/input/sanity/syntax_err.in` (empty file)
   - File: `test/log/sanity/syntax_err.log`
     You must run `./alkyl test/code/sanity/syntax_err.aky` (first build the compiler using `make` if needed) to see what compilation error output is generated. Strip ANSI escape colors from the output and write it to `test/log/sanity/syntax_err.log`.
6. Run `make` and run the tests using `./scripts/run_tests.sh` to verify that both `sanity/hello` and `sanity/syntax_err` pass successfully.
7. Write a completion report (`handoff.md`) in your working directory summarizing what you did, the changes made to `scripts/run_tests.sh`, and the output of running `./scripts/run_tests.sh sanity/`.

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A Forensic Auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.
