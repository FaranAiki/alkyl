# TODO

## 1. Class & Vector is Error!
Refer to test/code/cast/class_cast.kyl

It should be:
int main() {
  let gg = Vector[int](1, 3);
  // TODO: alkyl needs to be self-aware about the structs of typeof! fix this
  // cprintf (c"%d\n", typeof(gg));
  // cprintf(c"%d\n", gg as char); // expect wrong, yet get 16 for somereason 1 * 1 + 2 * 2 = 5
  cprintf(c"%d\n", gg as int); // this should be proper, inside as_int: x=1, y=3. then returns 1 * 1 + 3 * 3 = 10
  cprintf(c"%d, %d\n", gg.x, gg.y); // print 1, 3
}

Key points: Fix member access!
In repl/ethyl, this is proper
But in alkyl, this is not correct!

## 2. FINAL: to link these together. CHECK IF JIT COMPILER & AOT COMPILER ARE "100%" THE SAME (w/ diff abstraction)
Sometimes, the meta_vm (jit) can cause something different which is weird as the jit & aot should have the same output
