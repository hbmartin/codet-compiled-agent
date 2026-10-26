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

Cppcheck runs each translation unit in a separate project/cache. Coverage requires valid XML, a normal exit, no fatal/incomplete diagnostic, a successful checker completion report with semantic bounds/uninitialized checkers, and a nonempty semantic cache artifact. Progress messages and checker counts alone establish nothing. Each real gate additionally analyzes defective C and C++ bounds canaries, six defective/safe library-contract pairs through Codet's actual dependency headers, and a syntax-error canary with an inline suppression. Library-contract probes include only their library headers and omit application/toml++ search paths, avoiding unnecessary compatibility preprocessing. Probe expectations are explicit and mutually exclusive. Defect canaries must report the expected diagnostic attributed to their own source, safe canaries must have no noninformational diagnostics attributed to their source, and the syntax canary must fail analysis. Raw reports and completion proofs are retained.

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

`scripts/modern_sources.py` owns the maintained native inventory shared by analysis and
formatting. The compiler-database check fails if any application source lacks a command.
Semgrep runs directly against the production source/header roots with native-extension filters,
using the pinned CLI, strict analysis errors and findings-as-failure. It has no custom coverage
validator or rule-test fixtures. Named-file policies must be updated when files move; passing
Semgrep does not independently prove per-rule production targeting. Structural policies do not
establish authorization dominance, transaction ordering or race freedom.

Lint setup and commands are documented in [README.md](../README.md). `make test-lint-runners`
checks exact tool identity, missing tools, timeout and interruption handling, atomic publication
and stale success evidence, plus report/environment utilities and fake-tool Make failure handling. Analyzer-runner unit tests run in
the independent lint job; real analyzer detection remains in the native job.

CLI startup accepts construction-time cancellation and diagnostics through an additive
`StateStore::StartupOptions` overload. Contended migration waits emit one waiting diagnostic,
then check cancellation between nonblocking attempts at intervals no longer than 20 ms.
Existing constructors retain their interruptible blocking fallback. Lock trust checks and schema
version 8 are unchanged. Signal integration tests synchronize on migration diagnostics or a
blocked fixture request instead of assuming startup completes within a fixed delay.

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

Executed source hashes, suite results, tool versions and canary preprocessing evidence for this
follow-up are recorded in [enforcement validation](validation/lint-enforcement-followup.json).

Journal persistence, owned file diagnostics, fixture cleanup, interpreter separation, and
copy/hash optimizations are recorded in [journal and diagnostic validation](validation/journal-diagnostics-tooling-fixes.json).
That record includes five-run Release allocation/timing measurements, Debug and ASan/UBSan
results, and the remaining native Clang findings reproduced against unchanged main.
Journal call identity intentionally compares the sanitized stored JSON representation,
including repaired UTF-8 and `null` for non-finite numbers. File notices retain their complete
paths before publication; the CLI defers failed rendering until terminal restoration and
uses reversible byte escapes for emergency output.

Fullscreen replay, immediate headless file notices, and candidate-only journal batching are
covered by the [PR #30 follow-up validation](validation/pr30-notice-journal-followup.json).
It records regressions reproduced against `33fc5d7`, final sanitizer runs, and five-run Release
comparisons, including timing regressions alongside the allocation and SQLite work reductions.

Checked line-mode notice writes, Unicode control escaping, independent replay drains, and shared
terminal/receiver ownership are covered by the [notice-delivery follow-up validation](validation/notice-delivery-followup.json).
It includes Debug and ASan/UBSan suites, twenty diagnostic repetitions, and the remaining native
toolchain findings. Two alternating five-run Release comparisons rejected cross-write statement
caching: mixed-journal replacements did not improve in either set. The record retains the
experimental patch and links all [seventy measured medians](validation/notice-delivery-cache-medians.csv);
production candidate statements remain scoped to each write operation.

Stream recovery and checked notice flushing, exception-safe native terminal ownership,
selective invisible-character escaping, and shared host instrumentation are covered by
the [notice stream recovery validation](validation/notice-stream-recovery.json).
The record includes Debug and ASan/UBSan suites, twenty diagnostic repetitions, and
[53 Release allocation/work comparisons](validation/notice-stream-recovery-benchmark.csv).
The default benchmark retains its existing final `Glob` failure in both builds; its seven
preceding cases and all review/journal cases have identical allocation and work counters.
No timing improvement is claimed. Existing native Clang limitations remain explicit.

Recovery of ordinary stderr diagnostics after notice failures, checked `newterm()` startup,
lazy terminal restoration, and ASCII decoder fast paths are covered by the
[terminal and diagnostic recovery validation](validation/terminal-diagnostic-recovery.json).
It records fresh-process first-use/shutdown and nonblocking-stderr tests, real PTY startup
regressions, Debug and ASan/UBSan suites, and 800 focused diagnostic/lifecycle repetitions.
Two alternating five-run Release measurement sets compare all
[53 cases per set](validation/terminal-diagnostic-recovery-benchmark.csv), including raw timings,
allocation counts, allocated bytes, and SQLite work. The record retains an initial process-cleanup
test failure, focused reruns, and the successful full sanitizer retry. Existing Clang and legacy
benchmark limitations remain separate from the changes in this follow-up.

The [checked stdout and terminal cleanup record](validation/output-startup-followup.json)
documents Debug and ASan/UBSan suites, 1,060 focused repetitions, real pipe and PTY regressions,
and mutation checks that detect destructible sink storage and missing or late `endwin()` calls.
Its [53 existing allocation/work comparisons](validation/output-startup-followup-counters.csv)
retain identical counters against fetched main. These are smoke comparisons, with no latency
claim. Existing Clang findings and the legacy default-benchmark failure are recorded separately.

## Output delivery follow-up validation

The follow-up to #35 starts from fetched main `b4d768a`. Its source validation record is
[output-delivery-followup.json](validation/output-delivery-followup.json), with exact commands,
source and log hashes, test counts, initial validation corrections, and baseline analyzer comparisons.
Debug and ASan/UBSan each pass all nine CTest groups: 536 unit tests (534 passed and two expected
skips), 59 publication-fault tests, and 74 CLI integration tests. Twenty repetitions cover 1,040
focused diagnostic/lifecycle/output executions, 200 fault executions, and 100 CLI test executions,
for **1,340** total. Pinned dependencies, lint, Semgrep's locked frontend, runner checks, and analyzer
self-checks pass. Cppcheck completes 25/25 sources without findings. The same six Clang-Tidy
findings (including one old-SDK incomplete translation unit) and the same Clang Analyzer finding
remain; those gates are not clean. An initial ASan run had a child-setup failure in the unchanged
ownership-observation test; thirty isolated repetitions and the final full suite passed, and the
test was not changed.

The host-only `codet_export_bench` target measures a complete CLI export after fixture creation,
to `/dev/null`. Baseline and changed Release builds use identical harnesses and compiler settings,
one warm-up and five alternating measured runs per size, with no concurrent analyzers or tests.
[Export measurements](validation/output-delivery-followup-exports.csv) retain all summary counters;
the JSON record includes all 24 warm-up/measured samples.

| Export | Baseline median | Changed median | Baseline stream flushes | Changed descriptor writes |
| --- | ---: | ---: | ---: | ---: |
| 100 messages, 26,400 bytes | 1.723 ms | 1.382 ms | 100 | 1 |
| 10,000 messages, 10,320,000 bytes | 104.227 ms | 94.456 ms | 10,000 | 158 |

Allocation counts are unchanged (4,790 and 500,218); allocated bytes increase by 16 per invocation
for the larger diagnostic state. Stream flushes and intercepted descriptor calls are separate
observations, not a count of every hidden libc syscall. These are local timing observations, with
no timing-based CI threshold. Canonical JSON and ASCII text-repair implementation bytes are
unchanged. Linux and native Tiger runtime acceptance remain untested locally.

## Output cancellation follow-up validation

The follow-up to #36 starts from fetched main `dd65f13`. The
[validation record](validation/output-cancellation-followup.json) includes exact source hashes,
commands, test counts, analyzer findings, and initial failed runs. Debug and ASan/UBSan each pass
all nine CTest groups: 547 unit tests (545 passed and two expected skips), 62 publication-fault
tests, and 76 CLI integration tests. Twenty repetitions cover 1,260 focused unit executions,
260 fault executions, and 140 CLI executions, for **1,660** total. Pinned lint, Semgrep's locked
frontend, runner checks, and analyzer self-checks pass. Cppcheck completes 25/25 sources with no
findings. The same six Clang-Tidy findings (24/25 sources complete with the SDK13 wrapper) and
one Clang Analyzer finding remain; those gates are not clean.

An intermediate sanitizer run failed the unchanged one-shot child-setup test; thirty isolated
repetitions and the final full suite passed. One earlier CLI repetition timed out at the new
background helper's startup gate. The supervisor now closes unused pipe ends and reports early
child exits/stops with captured startup output. A subsequent repetition exposed a parent
`setpgid()`/child exec race in the active job-control fixture; the supervisor now verifies the
expected group before accepting that race. One hundred isolated line-mode helper runs, one
hundred active job-control runs, and the final repetitions passed. The initial startup timeout
and one-shot setup failure have no proven root cause; all failure logs are retained. No
assertions were weakened.

Two Release comparison sets each use one warm-up and five alternating measured runs, identical
harnesses and compiler settings, with no concurrent builds, tests, or analyzers. The
[whole-change medians](validation/output-cancellation-followup-performance.csv),
[isolated scalar/gathering medians](validation/output-cancellation-followup-gathering.csv), and
[all 528 warm-up/measured samples](validation/output-cancellation-followup-samples.csv) include
timing, allocations, allocated bytes, native write calls, and readiness calls. Export cases
exercise both `/dev/null` and regular files. The loopback response fixture and pipe drainer are
intrinsic benchmark helpers.

| Case | Set 1 baseline → changed | Set 2 baseline → changed | Work |
| --- | ---: | ---: | --- |
| 20,000 small JSONL records | 39.506 → 20.925 ms | 37.742 → 20.359 ms | 40,000 → 20,000 writes; zero allocations |
| 128 redirected 1 MiB answer boundaries | 3.222 → 0.150 ms | 3.009 → 0.153 ms | 128 → 0 allocations; 134,218,752 → 0 allocated bytes |
| 128 exhausted-buffer inputs of 1 MiB | 657.690 → <0.001 ms | 660.499 → <0.001 ms | 256 → 0 allocations; input is neither scanned nor copied |
| 2,000 large blocking-pipe records | 24.264 → 313.329 ms | 17.788 → 278.704 ms | 4,000 → 258,000 writes, plus 258,000 readiness calls |

The blocking-pipe cost is substantial: Darwin's runtime `PIPE_BUF` is 512 bytes. Smaller atomic
chunks and readiness checks provide bounded cancellation opportunities; no pipe throughput
improvement is claimed. Small blocking-pipe medians increase by 44% and 58%. Full HTTP-answer
allocation totals vary with curl chunking and buffer growth. The deterministic answer-boundary
measurement and allocation-fault/pointer-identity test establish removal of the redirected-answer
copy without relying on HTTP allocation counts.

Gathering is retained for multiple useful spans: its isolated JSONL medians improve by 41–47%
in both sets, with half the native write calls and unchanged allocations. Scalar paths remain for
single spans and single-vector batches. Large blocking-pipe gathering medians are about 1% slower
than scalar output with identical cancellation policy. Large regular-file export medians are
0.98% and 0.59% lower. Small regular-file medians are 0.85% and 0.15% higher, within overlapping
sample spreads and with mixed paired directions; paired median changes are approximately +0.02%
and −1.06%. This is treated as noise rather than a repeatable file-export slowdown. Operation and
allocation counts remain identical. No general export speedup or timing-based CI gate is claimed.

Canonical sanitization and text-repair code is unchanged apart from the shared decoder's
`string_view` parameter. Optimized assembly retains ASCII decoder bypasses. Local validation is
macOS arm64; Linux and native Tiger runtime acceptance remain untested. Existing local compiler
warning exceptions, SDK limitations, and the legacy default benchmark's `Glob` failure are
recorded separately.

## Diagnostic shutdown and terminal ownership follow-up

The [validation record](validation/diagnostic-shutdown-followup.json) starts from fetched main
`f79754155f6a3fd7a72dd412570880a28799aadc`. It includes the original shared-stderr hang,
socket cancellation, failed-detachment descriptor reuse, and separator-identity probes. The
reviewed PR #37 tree is identical to that baseline. Final checks cover checked descriptor
stderr, cancellation with shared full pipes/sockets, literal approval identifiers, private
terminal ownership/quarantine, and unchanged persistence/acknowledgement contracts.

[Release medians](validation/diagnostic-shutdown-followup-performance.csv) and
[every warm-up/measured sample](validation/diagnostic-shutdown-followup-performance-samples.csv)
compare identical harnesses and Apple Clang 21 settings. Each set has one warm-up and five
alternating measured runs; the whole set is repeated without concurrent analyzers. The
mandatory build disables only the optional private output buffer in source commit `800bfe1`;
the buffer build's native header exactly matches production. Small exports execute 50 complete
invocations per sample; large exports execute three. Timings are workload measurements, not
CI thresholds or a general CLI speedup claim.

| Workload | Baseline medians, sets 1 / 2 | Mandatory medians | Buffered medians |
| --- | --- | --- | --- |
| 20,000 private libedit refreshes | 1823.683 / 2246.209 ms | 1850.415 / 1899.430 ms | 84.223 / 92.099 ms |
| 2,000 large pipe records, no cancellation context | 270.453 / 302.420 ms | 18.641 / 22.730 ms | 19.444 / 21.461 ms |
| 50 small regular-file exports | 62.855 / 60.340 ms | 56.993 / 63.878 ms | 56.384 / 67.273 ms |
| Three large regular-file exports | 295.879 / 286.325 ms | 296.715 / 288.578 ms | 295.154 / 287.679 ms |

The 4 KiB private output buffer is retained: both editor sets improve by about 95%, with zero
C++ allocations in the measured loop and no repeated export regression. Input and the private
error stream remain unbuffered. The buffer adds 4096 owned bytes at native acquisition.
The mandatory invocation context adds eight bytes to an existing diagnostic-writer allocation,
without adding an allocation. Export write counts and payload bytes remain unchanged.
Without cancellation, large pipe records drop from 258,000 writes and readiness operations
to 2,000 writes and zero readiness operations. Cancellable pipe limits remain in force.

[Exploratory medians](validation/diagnostic-shutdown-followup-investigation-performance.csv)
and [samples](validation/diagnostic-shutdown-followup-investigation-performance-samples.csv)
also include lazy descriptor checking. It is omitted: the existing fd-replacement regression
fails when checks are spaced out. Permission-only changes now preserve pending separators.
Native diagnostic writes retain two `fstat` calls per attempt (identity and classification),
versus one in the previous ostream path. Explicit write and `fstat` counter positive controls
pass. Libedit's internal libc writes and C allocations are not fully intercepted; zero editor
write-counter readings mean unavailable instrumentation, not zero native I/O. Longer diagnostic
and JSONL control runs are included in the JSON record; small timing differences are noisy.

Emergency pipe/socket output remains immediate best effort, with complete path formatting but
no promise of delivery or exactly-once visible output. Blocking TTY writes remain outside the
bounded-cancellation guarantee. Failed private-stream detachment purges pending output and
retains one complete ownership bundle until process exit; it skips native teardown and further
acquisition, rather than closing a descriptor behind its `FILE*`. Normal cleanup retains
`endwin()` before `delscreen()`. macOS sends additionally use the Tiger-era XNU `MSG_NBIO` ABI:
[historical send-buffer handling](https://github.com/apple-oss-distributions/xnu/blob/xnu-792/bsd/kern/uipc_socket.c)
and [current handling](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/uipc_socket.c).

Debug and ASan/UBSan CTest, repeated diagnostic/fault/PTY checks, pinned lint, Semgrep, runner
checks, and analyzer self-checks are recorded separately. Cppcheck is clean. Clang-Tidy retains
the same six recorded findings and one SDK13-incomplete translation unit; Clang Analyzer retains
the existing `context.cpp` division-by-zero finding. No checks or suppressions were weakened.
Linux and native Tiger runtime checks were not performed locally; the Tiger compiler and SDK
are unavailable on this host.
