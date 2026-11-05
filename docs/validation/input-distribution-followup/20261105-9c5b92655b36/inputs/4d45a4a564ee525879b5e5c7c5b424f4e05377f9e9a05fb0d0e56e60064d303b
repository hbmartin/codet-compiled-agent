"""Invocation-local, explicitly invalidated inspection of Tiger Mach-O files."""

import struct
from pathlib import Path


class MachO:
    def __init__(self, output, prefix):
        self.output, self.prefix, self.cache = output, prefix, {}

    def invalidate(self, path):
        self.cache.pop(str(path), None)

    def linkage(self, path):
        path = Path(path)
        stat = path.stat()
        key = (stat.st_ino, stat.st_size, stat.st_mtime_ns)
        cached = self.cache.get(str(path))
        if cached is None or cached[0] != key:
            cached = key, self.output(self.prefix + "otool", "-L", str(path))
            self.cache[str(path)] = cached
        return cached[1]

    def dependencies(self, path):
        return [line.strip().split(" ", 1)[0] for line in self.linkage(path).splitlines()[1:]]

    def load_commands(self, path):
        return self.output(self.prefix + "otool", "-l", str(path))

    @staticmethod
    def architecture(path, subtypes=None, kinds=(2, 6)):
        with Path(path).open("rb") as source:
            data = source.read(8 if subtypes is None else 16)
        if len(data) < 8 or struct.unpack(">II", data[:8]) != (0xFEEDFACE, 18):
            raise SystemExit("Non-PPC Mach-O: " + str(path))
        if subtypes is not None:
            if len(data) != 16:
                raise SystemExit("Truncated PPC Mach-O: " + str(path))
            _, _, subtype, kind = struct.unpack(">IIII", data)
            if subtype not in subtypes or kind not in kinds:
                raise SystemExit("Unexpected PPC750 Mach-O: " + str(path))
