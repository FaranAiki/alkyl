./build/ethyl << 'INNER'
let x = 10;
let ptr = &x;
ptr
*ptr
*ptr = 20;
x
INNER
