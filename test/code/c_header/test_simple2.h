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
