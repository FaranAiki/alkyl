# E2E Test Infra: Alkyl Compiler Architecture Refactoring

## Test Philosophy
- Opaque-box, requirement-driven verification for compiler architecture refactoring. No direct dependency on internal implementation structures.
- Methodology: 4-Tier test case design (Feature Coverage, Boundary/Corner Cases, Cross-Feature Pairwise Combinations, Real-World Application Workloads).

## Feature Inventory & Requirement Mapping
| # | Feature Area | Refactoring Requirement | Source Requirement | Tier 1 | Tier 2 | Tier 3 | Tier 4 |
|---|--------------|-------------------------|---------------------|:------:|:------:|:------:|:------:|
| F1 | String Interning & Identifiers | R1 (String Interning & Canonical Types) | ORIGINAL_REQUEST §R1 | 5 | 5 | ✓ | ✓ |
| F2 | Canonical Type System & Inferences | R1 (Canonical `VarType*` Pointers) | ORIGINAL_REQUEST §R1 | 5 | 5 | ✓ | ✓ |
| F3 | Bidirectional Type Checking | R2 (Expected Type Propagation) | ORIGINAL_REQUEST §R2 | 5 | 5 | ✓ | ✓ |
| F4 | Strict Traits & Interface Polymorphism | R2 (Strict Traits & `SemSymbol*`) | ORIGINAL_REQUEST §R2 | 5 | 5 | ✓ | ✓ |
| F5 | SIMD Lexing & AoS AST Control Flow | R3a (AoS AST & SIMD Token Scanner) | ORIGINAL_REQUEST §R3 | 5 | 5 | ✓ | ✓ |
| F6 | Multi-function & Parallel ALIR Lowering | R3b (Multi-threaded Thread Pool Lowering) | ORIGINAL_REQUEST §R3 | 5 | 5 | ✓ | ✓ |

## Test Architecture & Execution
- **Test Runner**: `scripts/run_tests.sh`
- **Invocation Options**:
  - Full suite (sequential): `./scripts/run_tests.sh`
  - Full suite (parallel): `./scripts/run_tests.sh --parallel`
  - Update golden outputs: `./scripts/run_tests.sh --update`
  - Target specific backend: `./scripts/run_tests.sh --llvm` / `--qbe` / `--mlir`
- **Directory Layout**:
  - Test source files (`.kyl`): `test/code/` (`refactor_r1/`, `test/code/refactor_r2/`, `test/code/refactor_r3/`, `test/code/tier3_combos/`, `test/code/tier4_workloads/`)
  - Expected stdout/stderr logs: `test/log/`
  - Expected execution output: `test/output/`
  - Temporary build outputs: `build/tmp/`

## Real-World Application Scenarios (Tier 4)
| # | Scenario File | Features Exercised | Domain Workload Description |
|---|---------------|--------------------|-----------------------------|
| 1 | `config_parser.kyl` | F1, F2, F3, F5, F6 | String key-value parsing, type coercion, default setting fallbacks |
| 2 | `math_solver.kyl` | F2, F3, F5, F6 | Vector dot product calculations, polynomial evaluation, recursive factorial |
| 3 | `data_pipeline.kyl` | F2, F3, F4, F6 | Active record filtering, array processing, aggregate sum reduction |
| 4 | `state_machine.kyl` | F2, F5, F6 | Event-driven enum state transitions with `switch`, `leak`, transition counting |
| 5 | `array_sort.kyl` | F2, F3, F5, F6 | Array bubble sorting and invariant verification logic |

## Coverage Thresholds & Summary
- Tier 1 (Feature Coverage): 30 test cases (5 per feature)
- Tier 2 (Boundary & Corner Cases): 30 test cases (5 per feature)
- Tier 3 (Cross-Feature Combinations): 6 test cases
- Tier 4 (Real-World Application Workloads): 5 test cases
- Baseline Pre-existing Regression Tests: 102 test cases
- **Total Test Cases**: 173 test cases
