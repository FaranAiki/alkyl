# Building the Alkyl Compiler

The Alkyl compiler supports multiple build systems and backends. This document explains how to build the project and how to use the various compiler backends.

## Prerequisites

Before building the project, ensure you have the following installed:
* A C compiler (e.g., GCC or Clang)
* **CMake** (v3.20+) or **Bazel** (v7.0+)
* **LLVM** (v15+) for the primary LLVM backend
* **MLIR** (part of LLVM, v15+) for the experimental MLIR backend
* **QBE** for the QBE backend
* **Rust** (and Cargo) for the experimental Cranelift backend
* Flex and Bison (optional, depending on the current parser stage, though Alkyl primarily uses a custom hand-written recursive descent parser)

---

## 1. Building with CMake (Primary)

CMake is the primary and most supported build system for Alkyl. It will automatically detect your system's LLVM, MLIR, and Rust environments and build all available compiler backends.

To build the project:

```bash
# 1. Create a build directory
mkdir build
cd build

# 2. Configure the project
cmake ..

# 3. Build the project
cmake --build . -j$(nproc)
```

The compiled binaries will be available in the `build/` directory.

---

## 2. Building with Bazel

Bazel is also supported as an alternative build system. It provides hermetic builds and tracks dependencies effectively.

To build the project using Bazel:

```bash
# Build the main LLVM compiler
bazel build //:alkyl_llvm

# Build the QBE compiler
bazel build //:alkyl_qbe

# Build the MLIR compiler
bazel build //:alkyl_mlir

# Build the Cranelift compiler
bazel build //:alkyl_cranelift

# Build the REPL/Driver
bazel build //:ethyl
bazel build //:alkyl
```

The compiled binaries will be located in the `bazel-bin/` directory.

---

## 3. Compiler Backends

Alkyl is modular and supports different backends for code generation. After building the project, you will have several executable binaries, each targeting a specific backend.

### Using the LLVM Backend (`alkyl_llvm`)
This is the default, most stable, and highest-performing backend. It lowers the Alkyl ALIR directly to LLVM IR.

```bash
# Compile and run a source file
./build/alkyl_llvm test/code/general/test_real_faran.kyl --opt
./build/out
```

### Using the QBE Backend (`alkyl_qbe`)
QBE is a lightweight and fast-compiling backend. It is designed to be minimal.

```bash
# Compile and run a source file
./build/alkyl_qbe test/code/general/test_real_faran.kyl --unopt
./build/out
```

### Using the MLIR Backend (`alkyl_mlir`)
This backend lowers Alkyl to MLIR dialects before finally generating machine code. It's currently experimental.

```bash
# Compile and run a source file
./build/alkyl_mlir test/code/general/test_real_faran.kyl --unopt
./build/out
```

### Using the Cranelift Backend (`alkyl_cranelift`)
This backend uses a Rust-based wrapper to interface with Cranelift, a fast JIT-capable code generator from the Wasmtime project. It's currently experimental.

```bash
# Compile and run a source file
./build/alkyl_cranelift test/code/general/test_real_faran.kyl --unopt
./build/out
```

---

## 4. Optimization Flags

When running the compilers, you can pass optimization flags:
* `--opt`: Enables optimizations (e.g., `-O3` equivalent for LLVM/MLIR/Cranelift/QBE). This makes compilation slightly slower but produces highly optimized binaries.
* `--unopt`: Disables optimizations for faster compilation times and better debugging experiences.

## 5. Object Output (Only Compile)

The generic driver `alkyl` supports standard compiler flags, such as `-c`, to output object files without linking them into an executable. This allows you to build Alkyl libraries that do not require an `int main()` function.

```bash
./build/alkyl -c my_lib.kyl -o my_lib.o
```

## 6. Testing and Benchmarking

Alkyl includes automated tools for verifying compiler correctness and performance.

### Running Tests
The official test runner script `scripts/run_tests.sh` executes all `.kyl` test files under `test/code/` across the chosen compiler backends and optimization levels (`--opt` and `--unopt`). It verifies that the compilation succeeds (or fails as expected), the program executes correctly, and the output logs match the expected output.

```bash
# Run all tests using the default build/alkyl compiler in parallel
./scripts/run_tests.sh --parallel

# Run tests targeting only the LLVM backend
./scripts/run_tests.sh --llvm --parallel

# Update expected output logs for all tests
./scripts/run_tests.sh --update
```

### Benchmarking with compare_bin
The `scripts/compare_bin` utility uses `hyperfine` to comprehensively benchmark and compare the various backends (LLVM, QBE, MLIR, Cranelift). It measures compilation time, execution time, output binary size, and cache performance (using `valgrind`).

```bash
# Compare a specific test file across all backends over 50 runs
./scripts/compare_bin test/code/examples/mandelbrot.kyl --opt --runs 50
```
