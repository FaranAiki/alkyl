extern int printf(char*, ...) as cprintf;

int main() {
    let t = [[1, 2], [3, 4]];
    cprintf("%d %d %d %d\n", t[0][0], t[0][1], t[1][0], t[1][1]);
    return 0;
}
