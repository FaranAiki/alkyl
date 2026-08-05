# Testing Alkyl

Alkyl provides automated testing scripts to ensure the correctness of the compiler.

## Running Tests (`scripts/run_tests.sh`)
The `scripts/run_tests.sh` script automates the process of running test cases. It scans for test files, compiles them using `build/alkyl`, and compares their exit codes against the expected outcomes. It helps in catching regressions quickly.

## Comparing Binaries (`compare_bin`)
The `compare_bin` script is used to test the output of different backends (LLVM, MLIR, QBE, Cranelift). It compiles the same code using different backends and verifies that their outputs and behaviors match. This is crucial to ensure that the code behaves consistently regardless of the backend chosen.
