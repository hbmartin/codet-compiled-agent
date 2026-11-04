"""Bounded supervision of a subprocess and the process group it owns."""
import os
import shutil
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


def system_ps():
    path = shutil.which("ps", path=os.defpath)
    if path is None:
        raise ValueError("System process inspection unavailable")
    return path


def group_inactive(group):
    # Darwin can return EPERM for a group containing only unreaped zombies. Establish
    # that state explicitly; permission failure alone never proves successful cleanup.
    output = subprocess.check_output([system_ps(), "-axo", "pgid=,stat="], text=True, timeout=2)
    members = []
    for line in output.splitlines():
        values = line.split()
        if len(values) != 2 or not values[0].isdigit():
            raise ValueError("Malformed cleanup process-group inspection")
        if int(values[0]) == group:
            members.append(values[1])
    return all(state.startswith("Z") for state in members), members


def stop_group(process, grace=2.0):
    if process.pid <= 0:
        raise ValueError('Cannot supervise a non-positive process group')
    observation = dict(group=process.pid, signals=[], reaped=False)
    process.group_cleanup = observation
    failure = None
    try:
        for action in (signal.SIGTERM, signal.SIGKILL):
            try:
                os.killpg(process.pid, action)
                observation['signals'].append(dict(signal=int(action), outcome='delivered'))
            except ProcessLookupError:
                observation['signals'].append(dict(signal=int(action), outcome='absent'))
            except PermissionError:
                inactive, members = group_inactive(process.pid)
                observation['group_states'] = members
                if not inactive:
                    raise
                observation['signals'].append(dict(signal=int(action), outcome='no-live-members'))
            if action == signal.SIGTERM:
                time.sleep(min(0.025, grace))
                inactive, members = group_inactive(process.pid)
                observation['group_states'] = members
                if inactive:
                    break
                deadline = time.monotonic() + grace
                # Keep the leader unreaped until escalation, preventing reuse of its PID.
                while time.monotonic() < deadline:
                    time.sleep(min(0.025, max(0, deadline - time.monotonic())))
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        failure = error
    finally:
        try:
            result = process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill() # Reap our own leader even if group cleanup was denied.
            result = process.wait(timeout=2)
            if failure is None:
                failure = RuntimeError('Owned process group did not terminate before cleanup deadline')
        observation.update(reaped=True, returncode=result)
    if failure is not None:
        failure.reaped_returncode = result
        raise failure
    return result


def run(argv, *, timeout, check=False, capture_output=False, input=None, cleanup_grace=2.0, **kwargs):
    if capture_output:
        if 'stdout' in kwargs or 'stderr' in kwargs:
            raise ValueError('capture_output conflicts with explicit streams')
        kwargs.update(stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    with interruption():
        try:
            process = subprocess.Popen(argv, start_new_session=True, **kwargs)
        except OSError as error:
            error.process_launch_failure = True
            raise
        try:
            stdout, stderr = process.communicate(input=input, timeout=timeout)
        except BaseException as error:
            try:
                error.returncode = stop_group(process, cleanup_grace)
            except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as cleanup:
                error.returncode = getattr(cleanup, "reaped_returncode", process.returncode)
                error.cleanup_failure = str(cleanup)
            error.process_cleanup = getattr(process, "group_cleanup", {})
            raise
    result = subprocess.CompletedProcess(argv, process.returncode, stdout, stderr)
    if check:
        result.check_returncode()
    return result
