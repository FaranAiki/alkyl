#ifndef TEST_H
#define TEST_H

#include <stddef.h>

typedef int MyInt;
typedef unsigned long MyUlong;

struct Point {
    int x;
    int y;
};

union Data {
    int i;
    float f;
    char str[64];
};

enum Color {
    RED,
    GREEN,
    BLUE
};

extern int add(int a, int b);
extern void print(const char *msg);
extern __stdcall int win_api_func(int x);
extern __cdecl double compute(double x, double y);

#define MAX_SIZE 1024
#define PI 3.14159

#ifdef _WIN32
    #define WINDOWS_API 1
#else
    #define WINDOWS_API 0
#endif

#endif
