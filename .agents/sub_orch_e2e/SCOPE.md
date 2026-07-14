# Scope: E2E Testing Track

## Architecture
- Test Suite directory: `test/code/`
- Test Runner: A robust script `scripts/run_e2e_tests.py` or wrapper that invokes the Alkyl compiler, compiles test `.aky` files to `./out`, runs `./out` with corresponding `.in` inputs, and validates stdout/stderr against `.out` expected outputs and compiles logs against `.log` expected compilation logs.
- Test Cases structure:
  - `test/code/<feature>/<name>.aky` - Alkyl code
  - `test/input/<feature>/<name>.in` - Standard input
  - `test/output/<feature>/<name>.out` - Expected stdout/stderr
  - `test/log/<feature>/<name>.log` - Expected compilation log (if compilation failure is expected or warning checked)

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| 1 | Test Harness & Infra | Design and implement the test runner and folder structure | None | DONE (Conv: f74caa40-5427-44f6-ba91-ad661b49c9c1) |
| 2 | Tier 1 Tests | Implement 20+ feature coverage test cases (5 per feature) | M1 | PLANNED |
| 3 | Tier 2 Tests | Implement 20+ boundary and corner test cases (5 per feature) | M2 | PLANNED |
| 4 | Tier 3 Tests | Implement 4+ cross-feature combination test cases | M3 | PLANNED |
| 5 | Tier 4 Tests | Implement 5+ real-world application scenarios | M4 | PLANNED |
| 6 | Publish E2E Suite | Verify everything runs, generate `TEST_READY.md` and `TEST_INFRA.md` | M5 | PLANNED |

## Interface Contracts
- **Test Runner Protocol**: The runner must return exit code 0 if and only if all tests pass. For any test failure (compilation mismatch, compilation failure when success is expected, run failure, stdout mismatch), it must output detailed diffs and return exit code 1.
- **Negative Tests**: Negative tests (expecting compilation failures) are verified by checking that `alkyl` returns a non-zero exit code and its stderr matches the expected `.log` file (or contains specific expected error messages).
