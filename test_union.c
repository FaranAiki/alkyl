#include <stdio.h>
typedef union {
  int int_val;
  long long long_long_val;
} Value;
int main() {
  Value v;
  v.long_long_val = 0;
  v.int_val = -1;
  printf("%lld\n", v.long_long_val);
  return 0;
}
