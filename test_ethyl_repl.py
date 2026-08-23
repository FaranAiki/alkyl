import pty
import os
import sys

master, slave = pty.openpty()
pid = os.fork()
if pid == 0:
    os.close(master)
    os.dup2(slave, 0)
    os.dup2(slave, 1)
    os.dup2(slave, 2)
    os.close(slave)
    os.execl("./build/ethyl", "./build/ethyl")
else:
    os.close(slave)
    def read_until(fd, string):
        buf = ""
        while string not in buf:
            try:
                buf += os.read(fd, 1).decode("utf-8")
            except:
                break
        return buf

    out = read_until(master, "In [0]: ")
    print(out)
    os.write(master, b"namespace some_ns {\r")
    out = read_until(master, "In [0]: ")
    print(out)
    os.write(master, b"    int a() {\r")
    out = read_until(master, "In [0]: ")
    print(out)
    os.write(master, b"        return 0\r")
    out = read_until(master, "In [0]: ")
    print(out)
    os.write(master, b"    }\r")
    out = read_until(master, "In [0]: ")
    print(out)
    os.write(master, b"}\r")
    
    # Send EOF (Ctrl+D)
    os.write(master, b"\x04")
    
    out = read_until(master, "In [1]: ")
    print(out)

