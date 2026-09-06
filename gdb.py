import gdb

class SemErrorBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super(SemErrorBreakpoint, self).__init__("sem_error", internal=False)

    def stop(self):
        msg = gdb.parse_and_eval("msg").string()
        print(f"GDB_ERROR_HOOK: {msg}")
        return False

SemErrorBreakpoint()
