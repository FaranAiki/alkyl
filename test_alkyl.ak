extern int printf(char*, ...) as cprintf;

int main() {
    let t = ["a", "b"];
    cprintf("%s %s\n", t[0], t[1]);
    return 0;
}
