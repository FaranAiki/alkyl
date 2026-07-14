# BRIEFING — 2026-07-14T14:50:36Z

## Mission
Analyze how to implement R3: double_quote_as_string setting in LexerSettings and string parsing fallback behavior.

## 🔒 My Identity
- Archetype: Teamwork explorer
- Roles: Read-only investigator
- Working directory: /home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_lexer_3
- Original parent: 984ccd4d-afad-4943-952c-f0c6e85707cc
- Milestone: Lexer R3 Settings Analysis

## 🔒 Key Constraints
- Read-only investigation — do NOT implement
- Only write files within own folder /home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_lexer_3/

## Current Parent
- Conversation ID: 984ccd4d-afad-4943-952c-f0c6e85707cc
- Updated: 2026-07-14T14:50:36Z

## Investigation State
- **Explored paths**:
  - `include/lexer/lexer.h` (LexerSettings struct layout)
  - `src/lexer/lexer.c` (lexer_init defaults and double quote lexing behavior)
  - `src/parser/expr.c` (parsing of TOKEN_STRING and TOKEN_C_STRING)
  - `src/parser/top.c` (parsing of premeta block and applying settings)
  - `src/alir/lvalue.c` and `src/alir/utils.c` (lowering calls and literal nodes)
  - `scripts/run_tests.sh` (verification commands and process)
- **Key findings**:
  - Lexer must still yield `TOKEN_STRING` for double-quoted strings `"..."` so that other grammar parts (like imports, annotations) remain valid.
  - The fallback/desugaring behavior is perfectly suited to be handled in expression parsing (`parse_factor` in `src/parser/expr.c`) by querying `p->l->settings.double_quote_as_string`.
  - When `true` (default), `"..."` expands in-AST to a `CallNode` invoking `string` constructor with a nested `LiteralNode` containing the C-string (`char*` ptr_depth 1).
  - When `false`, `"..."` parses directly to a C-string `LiteralNode` (`char*` ptr_depth 1).
- **Unexplored areas**: None

## Key Decisions Made
- Carry out the AST expansion at the parser level instead of the lexer level to avoid breaking direct `TOKEN_STRING` lookups.

## Artifact Index
- /home/faranaiki/Git/alkyl/.agents/teamwork_preview_explorer_lexer_3/handoff.md — Final investigation report
