# Modern host static analysis

`make analyze-modern` runs Clang-Tidy, Cppcheck and Clang Static Analyzer. The shared source
manifest covers `src/modern/*.cpp` and `src/text_compat.c` (including the reliability and capture-catalog sources).
Project headers are checked through those sources. Tests, third-party implementations and
recovered C are excluded as standalone analysis inputs. Historical recovered-C findings remain in [ANALYSIS.md](ANALYSIS.md); its retired local harness is separate from these current checks.

## Setup and commands

On Ubuntu 24.04, install:

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential cmake ninja-build python3 python3-yaml ca-certificates \
  clang-18 clang-tidy-18 libcurl4-openssl-dev libssl-dev \
  zlib1g-dev libsqlite3-dev libncurses-dev libedit-dev
make fetch-deps
make fetch-analysis-tools  # Network; checksum-verified Cppcheck 2.22.0 source
make build-analysis-tools  # Offline host build
make test-modern-analysis-runner
make analyze-modern-selftest MODERN_CC=clang-18 MODERN_CXX=clang++-18
make analyze-modern MODERN_CC=clang-18 MODERN_CXX=clang++-18
make modern-analysis-host MODERN_CC=clang-18 MODERN_CXX=clang++-18
ctest --test-dir build/modern-checks --output-on-failure
```

The modern analysis targets configure `build/modern-checks` with Clang and Ninja, independently
of `build/host`. Normal host and Tiger builds retain their compiler settings. On another host,
install Clang, Clang-Tidy, Ninja and Python's PyYAML package alongside the normal host
dependencies. Defaults use `clang` and `clang++`; `CLANG_TIDY` and `CPPCHECK` select binaries strictly: a missing explicit selection fails without a PATH fallback. Cppcheck must report version 2.22.0. The default prefers the provisioned binary under `build/analysis-tools/cppcheck/bin`, then PATH.

Analyzer targets only configure their dedicated compilation database. They do not build the
application or GoogleTest. `modern-analysis-host` is the explicit build stage; it also compiles
both benchmark targets so their warning policy is checked in CI, without executing benchmark
workloads. Fetching analysis tools is separate from offline analysis; application dependency
pins are unchanged. Build the complete developer image from the repository root:

```sh
make toolchain-image  # Requires Docker and the pinned base toolchain image
docker build --no-cache --platform linux/amd64 -f cmake/Dockerfile.host-tests -t codet-host-tests:local .
```

Run this from the repository root; Docker must support linux/amd64 emulation on Apple Silicon and have network access during provisioning. The base `codet-tiger-build:gcc14` must exist locally. Its network-enabled provisioning installs Clang/Tidy 18, Python/PyYAML and checksum-pinned Cppcheck. The image builds Cppcheck with networking disabled. Analysis and tests can then run with `--network none`.

Individual targets are `analyze-modern-clang-tidy`, `analyze-modern-cppcheck`, and
`analyze-modern-clang-analyzer`. Configuration variables:

| Variable | Default |
| --- | --- |
| `MODERN_ANALYSIS_BUILD` | `build/modern-checks` |
| `MODERN_CC` / `MODERN_CXX` | `clang` / `clang++` |
| `MODERN_ANALYSIS_JOBS` | `4` |
| `MODERN_ANALYSIS_TIMEOUT` | `300` seconds per invocation |

To reuse an already configured compilation database without rebuilding:

```sh
python3 scripts/analyze-modern.py --build-dir build/modern-checks --tool all --jobs 4 --timeout 300
```

`--tool` also accepts `clang-tidy`, `cppcheck`, or `clang-analyzer`. The static analyzer requires
Clang compilation commands; a GCC-configured database fails explicitly. Missing or conflicting
commands for application sources also fail instead of silently reducing coverage.

## Checks and enforcement

- Every maintained CMake target, including foundation tests, main, the C helper,
  fault tests and benchmarks, uses `-Wall -Wextra -Wpedantic -Werror` plus
  `-Wconversion -Wsign-conversion`. Clang additionally enforces `-Wshorten-64-to-32`.
  These are target-local options; third-party targets do not inherit them.
- Clang-Tidy enables `bugprone-*` and `performance-*`, excluding
  `bugprone-easily-swappable-parameters`. `clang-analyzer-*` is excluded because the standalone
  analyzer runs separately. Readability and modernization checks are not enabled. The runner
  promotes enabled diagnostics to errors; this setting does not change legacy invocations.
- Cppcheck imports the filtered compilation database, uses C11/C++17 at exhaustive analysis depth,
  and enables errors, warnings, performance, portability and completion information. The pinned 2.22.0 built-in parser runs with `--safety`; the experimental Clang parser is excluded. Style and inconclusive
  checks are not enabled. Dependency-only findings are retained in the raw report but excluded
  from the application gate. Parser/internal failures always fail, regardless of location.
  The pass loads the pinned tool's POSIX, BSD, libcurl, OpenSSL, SQLite and zlib models.
  Each model has defective and safe real-header contract canaries: defects must be detected
  and safe callers must remain clean. Missing models or failed model detection cannot
  produce a passing manifest.
- Clang Static Analyzer runs its stable default checker set, including C++ checks. It preserves
  semantic compilation flags and removes object/dependency output flags. Diagnostics are read
  from valid plist output, including when Clang itself returns zero.

Every selected analyzer runs even when another fails. Findings and missing tools, crashes,
timeouts, invalid reports or incomplete source coverage make the overall command fail. Timeout
and Ctrl-C cleanup terminate owned analyzer process groups, escalate after 250 ms and bound draining/reaping to two seconds. Partial output is retained, disappearing groups are normal, queued work is cancelled, and interruption returns 130. A clean report establishes
only the checks and host configuration exercised; it is not a proof of correctness or PPC/Tiger
runtime compatibility.

Fix actionable findings. A genuine false positive may receive a check-specific, local suppression
with a nearby explanation of the relevant contract. Do not add blanket suppressions or an
accepted-warning baseline. Review changes to text helpers against their existing padding,
truncation, termination and return-value tests.

All analyzers attribute findings using resolved repository and source paths. A primary location or execution/call path in first-party code gates the run; translation-unit import identity alone does not. Dependency-only findings stay in reports. Every parser/internal failure, limit bail-out (including normal-depth branch/valueflow limits), crash, malformed report or missing completion record fails regardless of location. Inline suppression cannot hide a fatal Cppcheck failure: safety/critical-error evidence is checked separately.

Cppcheck runs each translation unit in a separate project/cache. Coverage requires valid XML, a normal exit, no fatal/incomplete diagnostic, a successful checker completion report with semantic bounds/uninitialized checkers, and a nonempty semantic cache artifact. Progress messages and checker counts alone establish nothing. Each real gate additionally analyzes defective C and C++ bounds canaries, six defective/safe library-contract pairs through Codet's actual dependency headers, and a syntax-error canary with an inline suppression. Defect canaries must report the expected diagnostic, safe canaries must have no noninformational diagnostics attributed to their source, and the syntax canary must fail analysis. Raw reports and completion proofs are retained.

Cppcheck's SimpleCpp preprocessor rejects toml++'s nested operator token-paste macro (also
reproduced with upstream Cppcheck 2.22). For this tool only, the runner copies the dependency
headers into build artifacts and rewrites `operator TOML_CONCAT(op, =)` as `operator op##=`.
Before using the copy, it preprocesses every affected translation unit with the real compiler
and both header trees. The outputs must match byte for byte with line markers removed and
`__FILE__` paths mapped back to the original tree. Any mismatch, preprocessing failure, or
unrecognized macro, empty replacement count or vacuous audit fails the scan. Include argv is normalized for split/attached flags, quoted paths, relative paths and symlinks before replacement. `compatibility.json` retains the header hashes, commands
and matching output hashes. Original dependency files are never changed or skipped.

The Cppcheck database also escapes quoted string definitions and promotes `-isystem` paths to
normal include lookup to preserve include/import semantics. Its standalone
flow analysis still runs on all application sources. The analyzer self-check exercises these
adapters with a quoted macro, a header path containing spaces, and nested token pasting.

## Evidence and CI

Reports are under `build/modern-checks/analysis/`: the aggregate manifest includes source hashes
and first-party header hashes plus per-tool results; each tool retains versions, commands, diagnostics and its filtered
compilation database. Clang tools write per-source logs and plist/YAML reports; Cppcheck writes
XML, progress logs and its analysis cache. These files are ignored build artifacts.

`make test-modern-analysis-runner` exercises selection, enforcement and failure handling with
temporary inputs and mocked tools. `make analyze-modern-selftest` additionally generates
deliberately defective C/C++ fixtures and checks that each installed tool detects the expected
problem. Those fixtures are analyzed only: they are never linked or executed.

`scripts/modern_sources.py` owns the maintained native inventory shared by analysis,
formatting and Semgrep. The compiler-database check continues to fail if any application source
lacks a command. Semgrep additionally parses maintained production headers and validates its
successful JSON against the expected file inventory; all checked-in rules must have passing,
nonvacuous fixtures. An always-applicable command-processor policy prevents the historical
file-specific rule filters from silently removing otherwise uncovered files. Reports retain
source/rule hashes; changes during a run fail. These structural rules do not establish
authorization dominance, transaction ordering or race freedom.

Lint setup and commands are documented in [README.md](../README.md). `make test-lint-runners`
checks missing tools, wrong versions, findings, malformed/empty reports, incomplete source/rule
inventories and stale success manifests without installing or invoking analyzers.

The `Modern checks` GitHub Actions workflow runs on Ubuntu 24.04 for pull requests, pushes to main and
manual dispatch. Feature-branch pushes do not duplicate PR runs. It installs Clang 18, provisions pinned Cppcheck 2.22.0, fetches checksum-verified dependencies,
builds with Clang, runs both forms of analyzer self-check, analyzes the application and runs
CTest's unit/CLI/integration suites. Independent lint and Semgrep jobs do not wait for C++
provisioning or a successful application build. The aggregate `Required checks` job fails unless
all mandatory jobs succeed, including when a dependency is skipped or cancelled. In the native
job, after a successful build, check steps run independently so
an earlier failure does not hide later results. Reports and test logs are uploaded even on
failure and retained for 14 days. Provider tests use hermetic fixtures rather than live credentials.

## Superseded coverage evidence

Earlier Cppcheck 2.13 passes are invalid as C++ coverage evidence. A dependency's suppressed syntax error caused the parser to abandon translation units after printing `Checking`, leaving empty caches and only four active checkers. Those runs did not analyze the application meaningfully. This pipeline replaces that evidence with source-hashed per-TU semantic completion records, real-header canaries and unsuppressible critical-failure checks. The 2.22.0 feasibility run exercised 125 active C++ checkers; the count is informative, never the coverage criterion. The installed 2.13 experimental Clang-parser probe crashed and is not an approved backend.
