## 2026-07-14T14:51:07Z

You are a Worker subagent for Milestone 1: Lexer Settings.
Your working directory is: /home/faranaiki/Git/alkyl/.agents/worker_lexer

Please implement the fix strategy for R3: double_quote_as_string setting and string parsing fallback.
Read the Explorer's findings and proposed diffs from: /home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_lexer_3/handoff.md

Instructions:
1. Update include/lexer/lexer.h to add double_quote_as_string to LexerSettings.
2. Update src/lexer/lexer.c to default double_quote_as_string to 1 in lexer_init.
3. Update src/parser/top.c to parse lexer.double_quote_as_string in premeta blocks.
4. Update src/parser/expr.c to transform TOKEN_STRING expressions based on double_quote_as_string:
   - When true: desugar "hello" into a CallNode string(c"hello").
   - When false: fallback to a LiteralNode C-string (base type TYPE_CHAR, ptr_depth = 1).
5. Build the project and run the existing tests to ensure no regressions.
6. Create a test file at test/string/test_lexer.aky that verifies double_quote_as_string is active by default and tests the fallback behavior when set to false. Make sure it follows existing testing patterns, compile and run it to verify, and add/update any expected output/log files so that ./scripts/run_tests.sh passes 100%.

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A Forensic Auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.
