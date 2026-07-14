# Orchestration Plan - Alkyl Compiler Extensions

## Objectives
Extend Alkyl compiler to support:
1. Advanced class instantiation and constructors (struct-like, custom named init)
2. Optional/default arguments in functions & named arguments in calls
3. Lexer settings (double_quote_as_string)

## Architecture Overview
- **Lexer**: Adjust `lexer.c` / `lexer.h` to support `double_quote_as_string` setting and `string(c"...")` conversion.
- **Parser**:
  - Extend function declarations parsing to accept default arguments.
  - Extend function call parsing to accept named arguments.
  - Extend class parsing to support custom constructor signatures (`init`, `void init`, `ClassName(...)`).
- **Semantic Analyzer**:
  - Infer implicit constructors and resolve defaults.
  - Match named arguments to parameter positions.
  - Ensure compatibility checks for default and named arguments.
- **ALIR Generation / Codegen**:
  - Generate IR for constructor functions.
  - Resolve default argument values at call-site if not passed.
  - Allocate and assign fields in struct-like instantiation.

## Dual-Track Strategy
We will launch two parallel sub-orchestrators:
1. **E2E Testing Track**:
   - Scope: Design test runner or use existing one, build a comprehensive test suite (Tiers 1-4) for lexer settings, named/default arguments, and class instantiation.
   - Outputs: `TEST_INFRA.md`, `TEST_READY.md`.
2. **Implementation Track**:
   - Scope: Lexer changes (Milestone 1) -> Default/named args (Milestone 2/3) -> Class instantiation (Milestone 4).
   - Verification: Pass 100% of E2E tests, followed by Adversarial Coverage Hardening (Tier 5).

## Milestones & Timeline
- Milestone 1: Lexer settings implementation (R3)
- Milestone 2: Optional/default arguments implementation (R2)
- Milestone 3: Named arguments implementation (R2)
- Milestone 4: Advanced class constructors and struct-like instantiation (R1)
- Milestone 5: E2E Verification & Adversarial Hardening (Tier 5)
