// This is a legacy in .c
// In Alkyl, one should write:
// 1. define DIAG_RED as "\033..."
// 2. define debug_flow as ...

#ifndef COMPILER_DEBUG_H
#define COMPILER_DEBUG_H

#define DIAG_RED    "\033[1;31m"
#define DIAG_GREEN  "\033[1;32m"
#define DIAG_YELLOW "\033[1;33m"
#define DIAG_BLUE   "\033[1;34m"
#define DIAG_PURPLE "\033[1;35m"
#define DIAG_CYAN   "\033[1;36m"
#define DIAG_GREY   "\033[0;90m"
#define DIAG_BOLD   "\033[1m"
#define DIAG_RESET  "\033[0m"

#define DEBUG_STEP
#ifdef DEBUG_STEP
  #define debug_step(msg, ...) fprintf(stderr, DIAG_CYAN "step: " DIAG_RESET msg "\n", ##__VA_ARGS__)
#else
  #define debug_step(msg, ...)
#endif // DEBUG_STEP

#define DEBUG_PARSER_OUT
#ifdef DEBUG_PARSER_OUT
  #define to_ast_out(p, a, f) parser_to_file(p, a, f)
#else
  #define to_ast_out(p, a, f)
#endif // DEBUG_PARSER_OUT

#define DEBUG_SEMANTIC_OUT
#ifdef DEBUG_SEMANTIC_OUT
  #define to_sem_out(p, f) semantic_to_file(p, f)
#else
  #define to_sem_out(p, f)
#endif // DEBUG_PARSER_OUT

// Unified debug macro - use debug_any for all debugging output
#define DEBUG_ANY
#ifdef DEBUG_ANY
  #define debug_any(msg, ...) fprintf(stderr, DIAG_CYAN "step: " DIAG_RESET msg "\n", ##__VA_ARGS__)
#else
  #define debug_any(msg, ...)
#endif // DEBUG_ANY

#endif // COMPILER_DEBUG_H
