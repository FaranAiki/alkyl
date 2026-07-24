#!/bin/bash
python3 -c '
import pty, os, sys
pid, fd = pty.fork()
if pid == 0:
    os.execv("./build/ethyl", ["./build/ethyl"])
else:
    import time
    time.sleep(0.5)
    os.write(fd, b"for t in [1, 2]\n    123\n\n")
    time.sleep(0.5)
    print(os.read(fd, 1024))
    os.write(fd, b"exit\n")
'
