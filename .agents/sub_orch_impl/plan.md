# Plan: Implementation Sub-Orchestration

This plan outlines the execution of the Alkyl Compiler Extensions Implementation Track, following the Project Pattern.

## Phase 1: Setup and Heartbeat
- [x] Create original request, BRIEFING.md, SCOPE.md, progress.md, and plan.md.
- [ ] Initialize heartbeat cron timer.

## Phase 2: Execute Milestone 1 (Lexer Settings)
- [ ] Dispatch 3 Explorer agents to analyze lexer config/settings (`double_quote_as_string`).
- [ ] Synthesize explorer findings.
- [ ] Dispatch 1 Worker to implement the lexer settings change and verify with tests.
- [ ] Dispatch 2 Reviewer agents to review the implemented changes.
- [ ] Dispatch 2 Challenger agents to stress test and verify correct behavior.
- [ ] Dispatch 1 Forensic Auditor to perform integrity audit.
- [ ] Run the Gate check. If pass, mark Milestone 1 DONE.

## Phase 3: Execute Milestone 3 (Optional/Default Args)
- [ ] Dispatch 3 Explorer agents to analyze parser/semantic/codegen changes for optional/default args.
- [ ] Synthesize explorer findings.
- [ ] Dispatch 1 Worker to implement.
- [ ] Dispatch 2 Reviewer agents.
- [ ] Dispatch 2 Challenger agents.
- [ ] Dispatch 1 Forensic Auditor.
- [ ] Run the Gate check. If pass, mark Milestone 3 DONE.

## Phase 4: Execute Milestone 4 (Named Arguments)
- [ ] Dispatch 3 Explorer agents to analyze named arguments parsing and lowering.
- [ ] Synthesize explorer findings.
- [ ] Dispatch 1 Worker to implement.
- [ ] Dispatch 2 Reviewer agents.
- [ ] Dispatch 2 Challenger agents.
- [ ] Dispatch 1 Forensic Auditor.
- [ ] Run the Gate check. If pass, mark Milestone 4 DONE.

## Phase 5: Execute Milestone 5 (Advanced Constructors)
- [ ] Dispatch 3 Explorer agents to analyze advanced constructors and struct-like instantiation.
- [ ] Synthesize explorer findings.
- [ ] Dispatch 1 Worker to implement.
- [ ] Dispatch 2 Reviewer agents.
- [ ] Dispatch 2 Challenger agents.
- [ ] Dispatch 1 Forensic Auditor.
- [ ] Run the Gate check. If pass, mark Milestone 5 DONE.

## Phase 6: Final Milestone (E2E Integration & Verification)
- [ ] Wait for `/home/faranaiki/Git/alkyl/TEST_READY.md` from the E2E Testing Track.
- [ ] **Phase 1**: Run the full E2E test suite (Tiers 1-4). Decompose test tiers as sub-milestones (Tier 1 -> 2 -> 3 -> 4). Dispatch Worker, Reviewer, and Auditor to fix any issues.
- [ ] **Phase 2**: Perform Adversarial Coverage Hardening (Tier 5). Dispatch 2 Challenger agents to find gaps and generate tests, then Worker to fix, and Reviewer/Auditor to verify.
- [ ] Final project verification.
- [ ] Send handoff report to parent.
