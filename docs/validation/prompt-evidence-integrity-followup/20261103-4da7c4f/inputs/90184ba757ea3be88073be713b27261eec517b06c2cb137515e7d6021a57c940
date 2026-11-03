"""Scoped PTY/process ownership and bounded collection; no terminal-library dependency."""
import contextlib
import errno
import fcntl
import os
import pty
import re
import select
import struct
import subprocess
import termios
import time
import unicodedata


def collect(test, master, received, predicate, seconds=10):
    deadline = time.monotonic() + seconds
    while not predicate() and time.monotonic() < deadline:
        if select.select([master], [], [], min(.02, max(0, deadline-time.monotonic())))[0]:
            try:
                chunk = os.read(master, 65536)
            except OSError as error:
                if error.errno != errno.EIO:
                    raise
                break
            if not chunk:
                break
            received.extend(chunk)
    test.assertTrue(predicate(), bytes(received))


class PtyProcess:
    def __init__(self):
        self._cleanup = contextlib.ExitStack()
        self.descriptors = []
        self.process = None

    def __enter__(self):
        return self

    def __exit__(self, *exception):
        return self._cleanup.__exit__(*exception)

    def close(self):
        self._cleanup.close()

    def _own(self, descriptor):
        try:
            self._cleanup.callback(os.close, descriptor)
        except BaseException:
            os.close(descriptor)
            raise
        self.descriptors.append(descriptor)
        return descriptor

    def openpty(self):
        master, slave = pty.openpty()
        try:
            self._own(master)
        except BaseException:
            os.close(slave)
            raise
        self._own(slave)
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH",24,80,0,0))
        return master,slave

    def pipe(self):
        reader, writer = os.pipe()
        try:
            self._own(reader)
        except BaseException:
            os.close(writer)
            raise
        self._own(writer)
        return reader,writer

    def start(self, *args, **kwargs):
        process = subprocess.Popen(*args, **kwargs)
        self.process = process
        try:
            self._cleanup.callback(self._stop, process)
        except BaseException:
            self._stop(process)
            raise
        return process

    @staticmethod
    def _stop(process):
        if process.poll() is None:
            process.kill()
        process.wait(timeout=5)


def line_cells(data):
    """Inspect CR/space row rewrites, including wide Unicode; discard libedit CSI controls."""
    text = re.sub(r'\x1b(?:\[[0-?]*[ -/]*[@-~]|\([A-Z0-9])', '', data.decode('utf-8', 'replace'))
    rows, row, column = [], {}, 0
    for point in text:
        if point == '\r':
            column = 0
        elif point == '\n':
            rows.append(''.join(row.get(i, ' ') for i in range(max(row, default=-1)+1)).rstrip())
            row, column = {}, 0
        elif point == '\b':
            column = max(0, column-1)
        elif ord(point) >= 32:
            width = 0 if unicodedata.combining(point) else 2 if unicodedata.east_asian_width(point) in ('W','F') else 1
            row[column] = point
            for cell in range(1, width):
                row[column+cell] = ''
            column += width
    rows.append(''.join(row.get(i, ' ') for i in range(max(row, default=-1)+1)).rstrip())
    return rows
