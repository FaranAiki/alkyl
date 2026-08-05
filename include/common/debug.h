// This is a legacy in .c
// In Alkyl, one should write:
// 1. define DIAG_RED as "\033..."
// 2. define debug_flow as ...

#ifndef COMPILER_DEBUG_H
#define COMPILER_DEBUG_H

#define COLOR
#ifdef COLOR
  #define DIAG_RED    "\033[1;31m"
  #define DIAG_GREEN  "\033[1;32m"
  #define DIAG_YELLOW "\033[1;33m"
  #define DIAG_BLUE   "\033[1;34m"
  #define DIAG_PURPLE "\033[1;35m"
  #define DIAG_CYAN   "\033[1;36m"
  #define DIAG_GREY   "\033[0;90m"
  #define DIAG_BOLD   "\033[1m"
  #define DIAG_RESET  "\033[0m"
#else
  #define DIAG_RED    ""
  #define DIAG_GREEN  ""
  #define DIAG_YELLOW ""
  #define DIAG_BLUE   ""
  #define DIAG_PURPLE ""
  #define DIAG_CYAN   ""
  #define DIAG_GREY   ""
  #define DIAG_BOLD   ""
  #define DIAG_RESET  ""
#endif // COLOR

#ifdef DEBUG_STEP
  #define debug_step(msg, ...) fprintf(stderr, DIAG_CYAN "step: " DIAG_RESET msg "\n", ##__VA_ARGS__)
#else
  #define debug_step(msg, ...)
#endif // DEBUG_STEP

#ifdef DEBUG_PARSER_OUT
  #define to_ast_out(p, a, f) parser_to_file(p, a, f)
#else
  #define to_ast_out(p, a, f)
#endif // DEBUG_PARSER_OUT

#ifdef DEBUG_SEMANTIC_OUT
  #define to_sem_out(p, f) semantic_to_file(p, f)
#else
  #define to_sem_out(p, f)
#endif // DEBUG_PARSER_OUT

#ifndef NDEBUG
  #define debug_any(msg, ...) fprintf(stderr, DIAG_CYAN "debug: " DIAG_RESET msg, ##__VA_ARGS__)
  #define debug_parser(msg, ...) debug_any("parser: " msg, ##__VA_ARGS__)
  #define debug_alir(msg, ...) debug_any("alir: " msg, ##__VA_ARGS__)
  #define debug_alick(msg, ...) debug_any("alick: " msg, ##__VA_ARGS__)
  #define debug_lexer(msg, ...) debug_any("lexer: " msg, ##__VA_ARGS__)
  #define debug_codegen(msg, ...) debug_any("codegen: " msg, ##__VA_ARGS__)
  #define debug_semantic(msg, ...) debug_any("semantic: " msg, ##__VA_ARGS__)
#else
  #define debug_any(msg, ...)
  #define debug_parser(msg, ...)
  #define debug_alir(msg, ...)
  #define debug_alick(msg, ...)
  #define debug_lexer(msg, ...)
  #define debug_codegen(msg, ...)
  #define debug_semantic(msg, ...)
#endif // NDEBUG

#endif // COMPILER_DEBUG_H
