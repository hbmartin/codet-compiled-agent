"""Bounded supervision of a subprocess and the process group it owns."""
import os
import signal
import subprocess
import threading
import time
from contextlib import contextmanager


@contextmanager
def interruption():
    # Only the owning main thread can change Python's handlers. Child groups never inherit
    # a different process-wide disposition; their exec restores Python's normal semantics.
    if threading.current_thread() is not threading.main_thread():
        yield
        return
    previous = signal.getsignal(signal.SIGTERM)
    def terminate(signum, frame):
        raise KeyboardInterrupt('Interrupted by SIGTERM')
    signal.signal(signal.SIGTERM, terminate)
    try:
        yield
    finally:
        signal.signal(signal.SIGTERM, previous)


def stop_group(process, grace=2.0):
    if process.pid <= 0:
        raise ValueError('Cannot supervise a non-positive process group')
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    deadline = time.monotonic() + grace
    # A terminated parent is not evidence that its descendants have exited. Give nested
    # supervisors their cleanup interval, then signal the still-owned group directly.
    while time.monotonic() < deadline:
        process.poll()
        time.sleep(min(0.025, max(0, deadline - time.monotonic())))
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    return process.wait(timeout=2)


def run(argv, *, timeout, check=False, capture_output=False, input=None, cleanup_grace=2.0, **kwargs):
    if capture_output:
        if 'stdout' in kwargs or 'stderr' in kwargs:
            raise ValueError('capture_output conflicts with explicit streams')
        kwargs.update(stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    with interruption():
        process = subprocess.Popen(argv, start_new_session=True, **kwargs)
        try:
            stdout, stderr = process.communicate(input=input, timeout=timeout)
        except BaseException as error:
            try:
                error.returncode = stop_group(process, cleanup_grace)
            except (OSError, subprocess.SubprocessError) as cleanup:
                error.cleanup_failure = str(cleanup)
            raise
    result = subprocess.CompletedProcess(argv, process.returncode, stdout, stderr)
    if check:
        result.check_returncode()
    return result
