"""Dependency-light checked adapters for immutable historical production variants."""
import os
import tempfile
from pathlib import Path


def cli_preparation_adapter(source):
    text = source.read_text()
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


def generate(source, output):
    _, adapter = cli_preparation_adapter(Path(source))
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() and output.read_text() == adapter:
        return
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", dir=output.parent, prefix=".adapter-", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(adapter)
        os.replace(temporary, output)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
