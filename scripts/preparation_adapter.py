"""Dependency-light checked adapters for immutable historical production variants."""
import hashlib
import os
import stat
import uuid
from pathlib import Path


def _extract(text):
    begin = "  const auto prepare_line = "
    end = "\n  const auto preparation_ready"
    if text.count(begin) != 1 or text.count(end) != 1:
        raise ValueError("CLI preparation excerpt is not unique")
    excerpt = text[text.index(begin):text.index(end)]
    adapter = '''#pragma once
#include "codet/detail/cli.hpp"
#include "codet/detail/request_input.hpp"
#include "codet/detail/submission_queue.hpp"
#if __has_include("codet/detail/interactive_preparation.hpp")
#include "codet/detail/interactive_preparation.hpp"
#endif
namespace codet::test {
inline detail::PreparationOutcome cli_prepare(Config &config, FileSystem &files,
    detail::InputLine &line, Interaction &queue, const Cancellation &cancel) {
  struct Borrowed {
    Interaction *value;
    Interaction *get() const { return value; }
    Interaction &operator*() const { return *value; }
  } interaction{&queue};
  detail::CliLifecycleOps lifecycle;
''' + excerpt + "\n  prepare_line(line);\n  return line.preparation;\n}\n}\n"
    return excerpt, adapter


def cli_preparation_adapter(source):
    return _extract(Path(source).read_text())


def _write_adapter(output, adapter):
    previous_mode = stat.S_IMODE(output.stat().st_mode) if output.exists() else None
    temporary = output.with_name('.adapter-' + uuid.uuid4().hex)
    descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o666)
    try:
        if previous_mode is not None:
            os.fchmod(descriptor, previous_mode)
        output_stream = os.fdopen(descriptor, "w", encoding="utf-8")
        descriptor = None  # The stream owns the descriptor from this point.
        with output_stream as stream:
            stream.write(adapter)
        os.replace(temporary, output)
    finally:
        if descriptor is not None:
            os.close(descriptor)
        temporary.unlink(missing_ok=True)


def generate(source, output):
    source, output = Path(source), Path(output)
    raw = source.read_bytes()
    text = raw.decode("utf-8").replace("\r\n", "\n").replace("\r", "\n")
    excerpt, adapter = _extract(text)
    output.parent.mkdir(parents=True, exist_ok=True)
    if not (output.exists() and output.read_text() == adapter):
        _write_adapter(output, adapter)
    return dict(source=str(source), source_sha256=hashlib.sha256(raw).hexdigest(),
                excerpt_sha256=hashlib.sha256(excerpt.encode()).hexdigest(),
                adapter=str(output), adapter_sha256=hashlib.sha256(adapter.encode()).hexdigest())
