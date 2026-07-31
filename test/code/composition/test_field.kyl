extern
  int printf(char*, ...) as cprintf;

class Identity {
  char *name;
  int age;

  int speak() {
    cprintf "Hello!\n";
    return 1;
  }
}

class Person has Identity {
  int intro() {
    cprintf "I am %d y.o.\n", this.age;
    cprintf "My name is %s\n", this[Identity].name;
    return 1;
  }
}

int main() {
  Person p = Person("Faran Aiki", 10);
  p[Identity].speak();
  p.intro();
  return 0;
}
