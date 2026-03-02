#ifndef COMMON_ALIASES_H
#define COMMON_ALIASES_H

typedef union {
  int int_val;
  unsigned int unsigned_int_val;
  long long_val;
  long long long_long_val;
  unsigned long long unsigned_long_val;
  float float_val;
  double double_val;
  char *str_val; 
  void *any;
} Value;

#endif // COMMON_ALIASES_H
