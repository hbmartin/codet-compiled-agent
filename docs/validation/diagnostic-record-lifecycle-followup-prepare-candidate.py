import pathlib,json,tarfile,io,subprocess
root=pathlib.Path.cwd();folder=root/'build/diagnostic-record-lifecycle-followup';src=folder/'optimized-src';src.mkdir(exist_ok=True)
ref=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
with tarfile.open(fileobj=io.BytesIO(subprocess.check_output(['git','archive',ref]))) as archive:archive.extractall(src,filter='data')
p=src/'src/modern/platform.cpp';s=p.read_text().replace('detail::OutputAttempt &attempt, bool presentation = false) noexcept', 'detail::OutputAttempt &attempt, bool presentation = false,\n                         std::ostream *stream = nullptr) noexcept')
s=s.replace('      reservation_ = native_destinations.admit(native_, presentation);', '''      if (stream) {
        auto *file = diagnostic_file(*stream);
        if (!*stream || (file && std::ferror(file)))
          (void)detail::recover_diagnostic_stream(*stream);
      }
      reservation_ = native_destinations.admit(native_, presentation);''')
a=s.index('    {\n      NativeMetadataGuard guard;',s.index('const auto inspection = inspect_native(native, attempt);'));b=s.index('    const bool presentation',a);s=s[:a]+s[b:]
s=s.replace('NativeDiagnosticRecord record(native, inspection, attempt, presentation);', 'NativeDiagnosticRecord record(native, inspection, attempt, presentation, &output_);');p.write_text(s)
p=src/'include/codet/detail/line_prompt.hpp';s=p.read_text().replace('  void paint() {','''  void open_row() noexcept {
    if (binding_.descriptor < 0)
      boundaries().open(terminal_);
  }
  void paint() {''').replace('    boundaries().open(terminal_);','    open_row();')
# Keep the local backend operation inside the new helper, rather than recursing.
s=s.replace('if (binding_.descriptor < 0)\n      open_row();', 'if (binding_.descriptor < 0)\n      boundaries().open(terminal_);')
a=s.index('    if (binding_.descriptor >= 0) {\n      const auto state',s.index('    paint();'));b=s.index('\n  }\n  void append',a);s=s[:a]+s[b:];p.write_text(s)
p=src/'include/codet/detail/output_destination.hpp';s=p.read_text().replace('    Reservation result{', '    if (presentation)\n      presented_ = true;\n    Reservation result{');p.write_text(s)
subprocess.run([str(root/'build/lint-venv/bin/clang-format'),'-i',str(src/'src/modern/platform.cpp'),str(src/'include/codet/detail/line_prompt.hpp'),str(p)],check=True)
variants=json.loads((folder/'variants.json').read_text());cmd=list(variants[1]['configure']);cmd[cmd.index('-S')+1]=str(src);cmd[cmd.index('-B')+1]=str(folder/'optimized-release')
with (folder/'optimized-configure.log').open('w') as out:subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT,check=True)
variants.append({'name':'optimized','ref':ref,'configure':cmd});(folder/'variants.json').write_text(json.dumps(variants,indent=2)+'\n')
