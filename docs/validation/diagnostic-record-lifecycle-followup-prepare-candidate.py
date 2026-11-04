transformations = []

def checked_replace(text, before, after, expected=1):
    count = text.count(before)
    if count != expected:
        raise ValueError('Historical transformation count mismatch: ' + repr(before))
    transformations.append(dict(before=before, count=count))
    return text.replace(before, after)
import argparse
_parser = argparse.ArgumentParser(description=__doc__)
_parser.add_argument('--folder', type=__import__('pathlib').Path, required=True)
_parser.add_argument('--output', type=__import__('pathlib').Path, required=True)
args = _parser.parse_args()
if args.output.exists():
    _parser.error('Output already exists; choose a new historical replay output')
import pathlib, json, tarfile, io, subprocess
root = pathlib.Path(__file__).resolve().parents[2]
folder = args.folder.resolve()
src = folder / 'optimized-src'
src.mkdir()
ref = json.loads((root / 'docs/validation/diagnostic-record-lifecycle-followup.json').read_text())['performance']['variants'][1]['ref']
with tarfile.open(fileobj=io.BytesIO(subprocess.check_output(['git', 'archive', ref], timeout=30))) as archive:
    archive.extractall(src, filter='data')
p = src / 'src/modern/platform.cpp'
s = checked_replace(p.read_text(), 'detail::OutputAttempt &attempt, bool presentation = false) noexcept', 'detail::OutputAttempt &attempt, bool presentation = false,\n                         std::ostream *stream = nullptr) noexcept')
s = checked_replace(s, '      reservation_ = native_destinations.admit(native_, presentation);', '      if (stream) {\n        auto *file = diagnostic_file(*stream);\n        if (!*stream || (file && std::ferror(file)))\n          (void)detail::recover_diagnostic_stream(*stream);\n      }\n      reservation_ = native_destinations.admit(native_, presentation);')
a = s.index('    {\n      NativeMetadataGuard guard;', s.index('const auto inspection = inspect_native(native, attempt);'))
b = s.index('    const bool presentation', a)
s = s[:a] + s[b:]
s = checked_replace(s, 'NativeDiagnosticRecord record(native, inspection, attempt, presentation);', 'NativeDiagnosticRecord record(native, inspection, attempt, presentation, &output_);')
p.write_text(s)
p = src / 'include/codet/detail/line_prompt.hpp'
s = checked_replace(checked_replace(p.read_text(), '  void paint() {', '  void open_row() noexcept {\n    if (binding_.descriptor < 0)\n      boundaries().open(terminal_);\n  }\n  void paint() {'), '    boundaries().open(terminal_);', '    open_row();', expected=6)
s = checked_replace(s, 'if (binding_.descriptor < 0)\n      open_row();', 'if (binding_.descriptor < 0)\n      boundaries().open(terminal_);')
a = s.index('    if (binding_.descriptor >= 0) {\n      const auto state', s.index('    paint();'))
b = s.index('\n  }\n  void append', a)
s = s[:a] + s[b:]
p.write_text(s)
p = src / 'include/codet/detail/output_destination.hpp'
s = checked_replace(p.read_text(), '    Reservation result{', '    if (presentation)\n      presented_ = true;\n    Reservation result{')
p.write_text(s)
subprocess.run([str(root / 'build/lint-venv/bin/clang-format'), '-i', str(src / 'src/modern/platform.cpp'), str(src / 'include/codet/detail/line_prompt.hpp'), str(p)], check=True, timeout=600)
variants = json.loads((folder / 'variants.json').read_text())
cmd = list(variants[1]['configure'])
cmd[cmd.index('-S') + 1] = str(src)
cmd[cmd.index('-B') + 1] = str(folder / 'optimized-release')
with (folder / 'optimized-configure.log').open('w') as out:
    subprocess.run(cmd, stdout=out, stderr=subprocess.STDOUT, check=True, timeout=600)
variants.append({'name': 'optimized', 'ref': ref, 'configure': cmd})
(folder / 'variants-candidate.json').write_text(json.dumps(variants, indent=2) + '\n')
if not (src / 'src/modern/platform.cpp').is_file():
    raise ValueError('Historical candidate source was not produced')
args.output.parent.mkdir(parents=True, exist_ok=True)
with args.output.open('x') as result:
    json.dump(dict(label='historical replay; not current campaign evidence', folder=str(folder), source_ref=locals().get('ref'), produced=[str(p) for p in folder.glob('*.json')]), result, indent=2)
