"""Build one declared harness snapshot against each immutable production variant."""
import hashlib
import json
import shlex
import shutil
import subprocess
from pathlib import Path

from evidence_inputs import sha256
from process_supervision import run as supervised_run

PROGRAMS = dict(line="codet_line_input_bench", output="codet_output_bench", export="codet_export_bench", preparation="codet_preparation_bench")


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


def verify_cli_adapter(record):
    source, adapter = Path(record["source"]), Path(record["adapter"])
    excerpt, expected = cli_preparation_adapter(source)
    if (record.get("source_sha256") != sha256(source) or
            record.get("excerpt_sha256") != hashlib.sha256(excerpt.encode()).hexdigest() or
            record.get("adapter_sha256") != sha256(adapter) or adapter.read_text() != expected):
        raise ValueError("CLI preparation adapter differs from verified production source")


def build_harnesses(root, folder, names, ninja):
    snapshot = folder / "harness-src"
    snapshot.mkdir()
    hashes = {}
    for name in names:
        target = snapshot / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(root / name, target)
        hashes[name] = sha256(target)
    variants = {}
    for variant in ("baseline", "changed"):
        build = folder / (variant + "-release")
        directory = folder / (variant + "-harness")
        directory.mkdir()
        database = json.loads((build / "compile_commands.json").read_text())
        source = (folder / "baseline-src" if variant == "baseline" else root) / "src/modern/cli.cpp"
        excerpt, adapter = cli_preparation_adapter(source)
        adapter_path = directory / "cli_prepare_adapter.hpp"
        adapter_path.write_text(adapter)
        adapter_record = dict(source=str(source), source_sha256=sha256(source),
                              excerpt_sha256=hashlib.sha256(excerpt.encode()).hexdigest(),
                              adapter=str(adapter_path), adapter_sha256=sha256(adapter_path))
        records = {}
        for name, target in PROGRAMS.items():
            original_target = PROGRAMS["output"] if name == "preparation" else target
            entry = next(e for e in database if e["file"].endswith("tests/" + {"line": "line_input", "output": "output", "export": "export", "preparation": "output"}[name] + "_bench.cpp"))
            compile_argv = list(entry.get("arguments") or shlex.split(entry["command"]))
            old_object = compile_argv[compile_argv.index("-o") + 1]
            obj = directory / (name + ".o")
            compile_argv[compile_argv.index("-o") + 1] = str(obj)
            compile_argv[compile_argv.index(entry["file"])] = str(snapshot / "tests" / ("preparation_bench.cpp" if name == "preparation" else Path(entry["file"]).name))
            compile_argv[1:1] = ["-I" + str(directory), "-I" + str(snapshot / "tests")]
            lines = subprocess.check_output([ninja, "-C", str(build), "-t", "commands", original_target], text=True, timeout=30).splitlines()
            link = shlex.split(lines[-1])
            if "&&" in link:
                link = link[link.index("&&") + 1:]
                if "&&" in link:
                    link = link[:link.index("&&")]
            binary = directory / target
            link[link.index(old_object)] = str(obj)
            link[link.index("-o") + 1] = str(binary)
            for argv in (compile_argv, link):
                supervised_run(argv, cwd=build, check=True, timeout=600)
            records[name] = dict(compile_argv=compile_argv, link_argv=link, library_sha256=sha256(build / "libcodet_core.a"),
                                 executable_sha256=sha256(binary), executable=str(binary))
        records["preparation"]["cli_adapter"] = adapter_record
        variants[variant] = records
    return dict(harness_hashes=hashes, variants=variants)
