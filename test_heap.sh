./build/ethyl << 'INNER'
import std.heap
let t = heap.alloc(10)
typeof t
t
heap.free(t)
INNER
