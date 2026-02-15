#ifndef COMPILER_DEBUG_H
#define COMPILER_DEBUG_H
#define COL_RESET   "\033[0m"
#define COL_GREEN   "\033[1;32m"
#define COL_BLUE    "\033[1;34m"
#define COL_RED     "\033[1;31m"
#define COL_CYAN    "\033[1;36m"
#define COL_YELLOW  "\033[1;33m"
#define INPUT_BUFFER_SIZE 4096

#define DEBUG_FLOw
#ifdef DEBUG_FLOW 
  #define debug_flow(msg, ...) printf(COL_BLUE "flow: " COL_RESET msg, ##__VA_ARGS__); putchar('\n');
#else 
  #define debug_flow(msg, ...) 
#endif

#define DEBUG_STEP 
#ifdef DEBUG_STEP 
  #define debug_step(msg, ...) printf(COL_BLUE "step: " COL_RESET msg, ##__VA_ARGS__); putchar('\n');
#else 
  #define debug_step(msg, ...) 
#endif

#endif
