"""Fresh, single-threaded controlling-terminal supervisor for startup regressions."""
import fcntl
import json
import os
import pty
import select
import signal
import sys
import termios
import time


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("unknown-term", "background-config"):
        print("terminal helper expects MODE CODET", file=sys.stderr)
        return 2
    mode, binary = sys.argv[1:]
    master, slave = pty.openpty()
    os.setsid()
    fcntl.ioctl(slave, termios.TIOCSCTTY, 0)
    os.tcsetpgrp(slave, os.getpgrp())
    signal.signal(signal.SIGHUP, signal.SIG_IGN)
    before = termios.tcgetattr(slave)
    command = [binary, "--ephemeral"]
    if mode == "background-config":
        command += ["-c", "keybindings.invalid=1"]
    environment = dict(os.environ, TERM="codet-invalid-terminal-33" if mode == "unknown-term" else "xterm")
    child = os.fork()
    if child == 0:
        if mode == "background-config":
            os.setpgid(0, 0)
        signal.signal(signal.SIGTTOU, signal.SIG_DFL)
        for descriptor in range(3):
            os.dup2(slave, descriptor)
        os.close(master)
        os.execve(binary, command, environment)
    result = {}
    output = bytearray()
    deadline = time.monotonic() + 10
    try:
        while time.monotonic() < deadline:
            if select.select([master], [], [], .02)[0]:
                output.extend(os.read(master, 65536))
            collected, status = os.waitpid(child, os.WNOHANG | os.WUNTRACED)
            if collected:
                if os.WIFSTOPPED(status):
                    result["stopped"] = os.WSTOPSIG(status)
                    os.kill(child, signal.SIGKILL)
                    os.waitpid(child, 0)
                else:
                    result["exit"] = os.waitstatus_to_exitcode(status)
                child = None
                break
        else:
            result["timeout"] = True
        while select.select([master], [], [], .02)[0]:
            output.extend(os.read(master, 65536))
        after = termios.tcgetattr(slave)
        # Darwin may set PENDIN while restoring otherwise identical settings.
        before[3] &= ~getattr(termios, "PENDIN", 0)
        after[3] &= ~getattr(termios, "PENDIN", 0)
        result["restored"] = before == after
        result["output"] = output.decode(errors="backslashreplace")
        print(json.dumps(result))
    finally:
        if child is not None:
            os.kill(child, signal.SIGKILL)
            os.waitpid(child, 0)
        os.close(master)
        os.close(slave)
    return 0


if __name__ == "__main__":
    sys.exit(main())
