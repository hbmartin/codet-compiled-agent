# Codet: a compiled agent targeting Tiger and PowerPC

This is a compiled agent that links with **GCC 14.2.0 for 32-bit big-endian PowerPC / Mac OS X 10.4 Tiger**. The default build produces `build/ppccode.ppc` from the application entry point, all 32 application subsystem files, restored global storage, and two compatibility helpers for bounded text operations.

Status: The build passes with `-Wall -Wextra -Wformat=2 -Werror` and zero warnings; runtime correctness has not been established. **The application has not been executed.** See [BUILD-NOTES.md](docs/BUILD-NOTES.md) for changes, library differences, and remaining limitations.

## Build

Install Make and Docker and start Docker. From this directory:

```sh
make pull    # Fetch the pinned GCC 14.2 / Tiger SDK image once, if needed.
make         # Compile, link, and statically verify the full application.
```

No application execution or installation target is provided. The linked binary is deliberately left non-executable (`0644`).

The pinned container supplies GCC 14.2.0, cctools, the Tiger CRT, and the Mac OS X 10.4u SDK. Flags select PowerPC G3/750 instructions, 32-bit pointers, big-endian data, and a 10.4 deployment target. These dependencies come from the image specified in `toolchain.mk`; they are not vendored into the source tree.

Compilation disables container networking and mounts only this directory. Source is read-only; `build/` is writable. Copying this directory is sufficient: the build never reads the original binary, a parent recovery directory, or an analysis export. No Python, jq, Ghidra, Rizin, or radare2 is required.

A matching cross toolchain can also use the inner Makefile directly:

```sh
make -f build.mk verify PPC_PREFIX=/path/to/powerpc-apple-darwin8- TIGER_SDK=/path/to/MacOSX10.4u.sdk
```

Verification intentionally requires GCC 14.2.0. Use `make -f build.mk app` when experimenting with another compatible compiler version.

## Targets and outputs

- `make` / `make app` / `make verify`: build the full application and check its architecture, deployment settings, SDK layouts, all 365 expected application symbols, and system library dependencies.
- `make check-sources`: syntax-check all 34 application translation units and save diagnostics in `build/diagnostics/`.
- `make analysis-image`: build the separate analysis image with GCC 14.2, Python, and Cppcheck; downloads packages during image construction.
- `make analyze`: run GCC's analyzer and Cppcheck on exactly `APP_SOURCES`. Findings are report-only; tool failures or reported analysis limits return nonzero. Both tools run even when the first reports incomplete coverage.
- `make analyze-gcc` / `make analyze-cppcheck`: run either analyzer separately; `make analyze-selftest` verifies detection using temporary defective fixtures that are never executed.
- `make analysis-extra-image` / `make analyze-extra`: build the separate free-tool layer and evaluate Clang Static Analyzer, Clang-Tidy, Flawfinder, Frama-C and Splint. Optional incompatibilities return nonzero with logs. Select a rerun with `ANALYSIS_EXTRA_TOOL=clang-analyzer`, `clang-tidy`, `flawfinder`, or `compatibility` (Frama-C/Splint).
- `make subset`: retain the separate 19-function static-library experiment and dummy-main toolchain probe. Neither is linked into the full app.
- `make check-text-host`: compile and run isolated tests of the two new text helpers against host libc. This links no application code and executes no PPC code. The default build does not run these tests.
- `make clean`: remove generated files under `build/`.

Static inspection records include `build/app-symbols-detailed.txt`, `build/app-imports.txt`, `build/dependencies.txt`, `build/load-commands.txt`, `build/macho-header.txt`, and `build/target-macros.txt`. `build/missing-functions.txt` must be empty for verification to pass.

Analyzer versions, commands, source hashes, the compiler-derived PPC32 platform configuration, preprocessed inputs, raw diagnostics, and coverage summaries are under `build/analysis/` (ignored by Git). Runs use a read-only source mount and no container network. Analysis objects are separate from production objects. The default per-command timeout is 300 seconds; override with `ANALYSIS_TIMEOUT=600` if needed. See [ANALYSIS.md](docs/ANALYSIS.md) for reviewed findings and coverage limitations. The analyzer image adds optional development dependencies; the normal build still needs only Make and Docker.

## Source organization

- `src/subsystems/`: application C grouped by subsystem, with original function addresses and recovery annotations.
- `src/storage.c`: initialized data, zero-filled storage, and native pointer relocation assignments.
- `src/text_compat.c`, `include/text_compat.h`: explicit truncation and padding contracts, preserving the recovered byte limits and libc return values.
- `include/app.h`, `app-functions.h`, `app-globals.h`: SDK integration, current cross-file declarations, and global storage views.
- `include/recovered-types.h`: inferred structures with compile-time size and offset assertions.
- `checks/layout.c`: compare recovered structures with the Tiger SDK.
- `checks/app-symbols.txt`: complete expected application function set for link verification.
- `src/subset.c`, `checks/link.c`: optional earlier subset experiment.

The four files `abi-semantics.c`, `recovered-storage.c`, `runtime_startup.c`, and `stdio_abi_stubs.c` remain as historical source annotations and are excluded from application compilation. `src/storage.c` replaces fixed-address storage; GCC and the Tiger SDK provide startup, loader support, and stdio entry points. Historical headers and assembly references in comments are not build dependencies.

## License

[GPL v3](LICENSE.md)
