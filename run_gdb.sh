#!/bin/bash
gdb -batch -ex "break *0x401177" -ex "run" -ex "info registers" ./build/out
