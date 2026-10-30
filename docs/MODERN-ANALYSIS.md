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
the persistence, sanitization and line-input benchmarks so their warning policy is checked in CI, without executing benchmark
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

## Approval review and exception-safe diagnostics follow-up

The [validation record](validation/approval-diagnostics-followup.json) starts from fetched main
`59395a0f583544c489817824b11be843d97ff3eb`. The original 80-column PTY probe displayed
“Approve?” without the command suffix. Fullscreen approval now owns a complete, separately
retained document with token-safe wrapping and sequential page review. Resizing restarts review;
confirmation requires a fresh answer after every page has been displayed. Tests cover narrow
screens, control bytes, Unicode, denial, EOF, cancellation, and a usable subsequent turn.

Cleanup reporting preserves the original exception, even when formatting or a stream exception
mask makes the secondary report fail. Worker quiescence precedes cancellation reset and terminal
restoration. Emergency notices and quarantine warnings share descriptor separator state and one
stack-owned interruption budget: eight EINTR retries across inspection, separator, path chunks,
and newline. EAGAIN remains immediate failure. Standalone writers without an invocation context
remain immediate best effort; active diagnostics retain interruptible pipe/socket waiting.

The quarantine regression uses the production 4 KiB buffer, verifies pending sentinel bytes,
and exercises normal process exit. It no longer calls `setvbuf` after native I/O or substitutes
separate backing storage. A fresh-process test checks quarantine-warning ordering after a failed
partial diagnostic. The shared diagnostic header has a compile regression against importing JSON. Host-only fault
shim tests are excluded from cross builds; a host compile without the interposer definition proves
that this test source carries no unresolved host-interposer dependency. This is not a Tiger check.

[Release medians](validation/approval-diagnostics-followup-performance.csv) and
[all warm-up/measured samples](validation/approval-diagnostics-followup-performance-samples.csv)
compare fetched main, correctness-only commit `6623174`, and the final optimized production code
at `be9214c`. All three use identical harnesses and compiler settings. Each set has one warm-up
and five alternating measured runs; the set is repeated without concurrent analyzers or tests.
Production source hashes match the measured descriptor implementation. Later changes affect only
the test launcher and this evidence.

| Workload | Baseline medians, sets 1 / 2 | Correctness-only medians | Optimized medians |
| --- | --- | --- | --- |
| 200,000 active native diagnostics | 639.657 / 530.412 ms | 626.258 / 533.026 ms | 423.507 / 384.060 ms |
| 200,000 standalone diagnostics | 608.184 / 533.182 ms | 622.755 / 537.772 ms | 431.128 / 387.178 ms |
| 200,000 pipe diagnostics | 561.184 / 532.241 ms | 571.757 / 531.440 ms | 439.795 / 392.369 ms |
| 50 small regular-file exports | 55.090 / 54.405 ms | 55.177 / 54.443 ms | 55.348 / 53.860 ms |
| Three large regular-file exports | 269.532 / 267.406 ms | 272.163 / 273.716 ms | 271.542 / 272.677 ms |
| 20,000 private editor refreshes | 80.853 / 80.805 ms | 82.159 / 81.264 ms | 81.390 / 80.445 ms |

Descriptor-inspection consolidation is retained. Each diagnostic still checks destination identity
and current flags; the identity stat also supplies classification. The loops use zero C++
allocations, halve `fstat` calls from 400,000 to 200,000, and reduce pipe-bound queries from
200,000 to one for an unchanged destination. Descriptor replacement invalidates the cache;
permission changes preserve it. Readiness/write work is unchanged. Positive controls validate
write, `fstat`, `fcntl`, and `fpathconf` counters. No lazy identity checking is included.

Export timing varies across overlapping samples. The optimized large-file medians are 0.23%
and 0.38% below the correctness-only build, while they are 0.75% and 1.97% above fetched main.
The corresponding correctness-only medians are 0.98% and 2.36% above fetched main. Qualification
compares the optional optimization with correctness-only: it adds no repeatable export or editor
slowdown. The full comparison and sample variability are retained; no general export speedup or
timing-based CI threshold is claimed.
Counters observe C++ `new` and direct native calls; libc/libedit internal writes and C allocations
are not fully intercepted. Unsupported variadic benchmark commands fail explicitly.

Both complete Debug and ASan/UBSan CTest suites pass: 564 unit tests with two expected opt-in
skips, 62 publication-fault tests, and 81 CLI integration tests. After the test-launch setup
change, complete integration suites are rerun in both builds. Twenty repetition batches cover
1,580 focused unit checks, 260 fault checks, and 180 CLI/PTY checks. Pinned dependencies are
verified/extracted offline; pinned lint, Semgrep, runner checks, and analyzer self-checks pass.
Cppcheck is clean. Clang-Tidy retains the same six findings and SDK13-incomplete translation unit;
Clang Analyzer retains the existing `context.cpp` division warning. No suppressions or checks
were weakened.

Failure evidence includes an early test/binary mismatch, corrected lint captures, and resolved
new analyzer warnings. Rare EPERM failures occurred before existing native helpers became ready;
400 direct background and 400 direct active probes did not reproduce them. The test launcher
now uses parent-owned process-group setup and an explicit pre-exec gate, retaining all real
job-control assertions. Final gated checks are reported separately from earlier failed batches. A later separator
regression exhausts inspection retries after a partial diagnostic: the previous production library
fails its negative control, while the final implementation preserves the owed newline and refuses
to classify from stale metadata. All full suites and repetitions were rerun after that fix.

Blocking TTY output remains outside bounded cancellation. Emergency output is best effort,
including after exhausting the interruption budget. Unicode policies, raw data outputs, storage,
journal authority, retained-notice ownership, and process-lifetime quarantine remain unchanged.
Linux and native Tiger runtime/toolchain checks are unavailable locally. Python 3.15 reports
existing SQLite fixture ResourceWarnings; the final suites pass.


## Locale-safe approval rendering follow-up

Fetched baseline `2030ab8` contains PR #39. A native C-locale negative control reproduces four
review rows spilling into their successors while the page is credited as reviewed, plus a bottom-row
input draw error. With the change, the same first page has no spilled rows, remains unapproved
because later pages are still required, and the input draw succeeds. Full-page tests independently
complete every page and reconstruct the entire prepared document before approval unlocks.

Curses prepares locale-compatible text before measuring it and probes native UTF-8 support on an
unrefreshed scratch pad. Unsupported bytes become reversible ASCII escapes; supported Unicode
remains intact. The approval document and byte-by-byte input use complete tokens and column
limits, with actual draw-row verification. Small screens retain input while approval stays disabled.
Ordinary narrow-curses questions, commentary and progress use the same reversible fallback;
wide-renderer text and prose tabs retain their behavior. The bounded UI transcript retains the request/context and its associated decision. Empty
submission is denial, distinct from cancellation. Identity validation precedes review and all gates
share the exact `y`/`yes` predicate.

The stat shim now shares platform-aware ABI selection across tests and benchmarks. Intel SDK
cross-compilation selects `fstat$INODE64` for both wrapper and resolver. Older glibc uses its
versioned stat entry point and forwards the version; local runtime coverage for that branch is
unavailable. File, pipe and socket positive controls pass on macOS arm64. Indeterminate pipe
bounds clear errno before each attempt, and all native diagnostic forms share logical record
boundaries and their retry budget. Conservative separators, including possible extra blank lines,
remain intentional.

[Recorded validation](validation/approval-rendering-followup.json),
[Release medians](validation/approval-rendering-followup-performance.csv), and
[every warm-up and measured sample](validation/approval-rendering-followup-performance-samples.csv)
include commands, source hashes, analyzer results and platform limits. Both full Debug and
ASan/UBSan CTest suites pass. Twenty batches exercise focused unit/fault tests and twelve CLI/PTY
cases. Pinned lint, Semgrep, runner checks and analyzer detection self-tests pass. Cppcheck reports
zero findings; Clang-Tidy retains the same six findings and SDK13-incomplete translation unit,
and Clang Analyzer retains the existing context.cpp division warning. No checks were weakened.

The Release comparison uses identical compiler settings and harnesses for fetched main and the
recorded changed production revision. Each of two sets has one warm-up and five alternating
measured runs, with no concurrent analyzers or test batches. Write/stat/flag/bound-query counters
have positive controls. Times below are medians in milliseconds; no timing-based CI threshold or
general performance improvement is claimed.

| Workload | Baseline, sets 1 / 2 | Changed, sets 1 / 2 |
| --- | --- | --- |
| diagnostics-amplified | 415.689 / 394.816 | 428.723 / 395.999 |
| diagnostics-standalone-amplified | 394.666 / 397.880 | 397.862 / 396.254 |
| diagnostics-pipe-amplified | 399.101 / 400.396 | 400.796 / 400.421 |
| jsonl-small | 20.001 / 19.875 | 19.959 / 19.933 |
| editor | 84.238 / 83.455 | 85.130 / 82.698 |
| export-small-file | 57.323 / 57.820 | 55.981 / 56.284 |
| export-large-file | 283.782 / 277.781 | 288.141 / 276.796 |

Diagnostic loops retain zero C++ allocations and the same write, identity, flag and bound-query counts. Export allocation/byte and output-operation counts are identical. Large-file export median changes versus baseline are +1.54% and -0.35% in the two sets. This is a correctness/consolidation change with no claimed export speedup.

Counters cover C++ new and direct observed calls, not every libc/libedit operation or C allocation.
Blocking TTY cancellation, standalone/emergency best-effort delivery, raw data output, Unicode
policy, storage formats and journal authority retain their previous contracts. Native Intel,
older-glibc and Tiger runtime/toolchain checks were unavailable locally; Ubuntu workflow evidence
is linked separately when available.

The first Ubuntu run compiled and passed static analysis, then exposed narrow-curses display expectations and an inherited-fd-hole assumption in a fault helper. Ordinary narrow displays now use reversible byte escapes; the helper reserves holes and deliberately inherits fd 64. No checks were removed or relaxed to accept garbled text or successful acquisition in a fault case. The initial workflow artifact is recorded alongside the replacement run.

## Visible approval and input ownership follow-up

Fetched baseline `e3b32c3` contains PR #40. Final production revision
`7f6a0bcd278b0700860850875cc68844b17643c9` makes approval targets/context and input
strictly visible without changing execution or answer bytes. A native shared-boundary negative
control approves a document containing a raw tag character and nine combining marks on baseline;
the changed document renders their bytes as complete ASCII escapes. Both versions preserve the
spacing-Unicode control and still allow approval after successful complete review. Real screen-cell
tests separately inspect retained wide characters, escapes, row limits and the reserved final column.

The strict policy uses 376 merged Unicode 17.0 ranges for default-ignorable characters and general
categories Mn, Me and Cf, plus locale printability/width checks. The checked-in table records official
source hashes and the Unicode license. There is no runtime Unicode dependency. Ordinary prose,
preserved-file formatting and canonical/structured data retain their existing policies. Native Linux
selects wide curses and requires actual UTF-8 cell coverage in CI; Darwin/Tiger discovery remains
unchanged. Scratch-pad failure selects the ASCII fallback.

One prepared drawing boundary clears and clips every curses row at complete rendered tokens.
Normal input, disabled small-screen input, headings, progress, transcript and approval rows all
reserve the final column. Drafts retain the request identity captured at their first byte through Enter.
Expired/rejected approval and question answers are discarded instead of submitted as ordinary user
messages. Ordinary partial messages survive worker completion. A deterministic gated regression
fails on baseline in both line and curses modes and passes with the change.

Rendered history has a 256 KiB payload cap and a 1,000-line cap. Each approval excerpt, including
labels and its omission marker, consumes at most 16 KiB; its decision carries the request identity.
The complete pending review is independent. Cached input keeps the original 65,536-byte limit,
rebuilds only the affected UTF-8 tail, and chooses its viewport from cached rendered-token metadata.
An allocation failure discards a partial cache, preserves the exception and permits fresh input.
Unchanged redraws copy only the visible suffix.

Failed unterminated diagnostics now conservatively owe a separator on both native and injected
paths; successful fragments keep concatenating. An indeterminate zero-errno pipe-bound query
caches the conservative bound for the descriptor identity. Interrupted/failed queries remain
uncached. Existing exception masks, eight-interruption budgets, destination rebinding, emergency
ownership and best-effort output policies remain intact.

[Validation evidence](validation/approval-visibility-followup.json),
[Release medians](validation/approval-visibility-followup-performance.csv), and
[all warm-up/measured samples](validation/approval-visibility-followup-performance-samples.csv)
record commands, final source hashes, compiler/harness identities, controls and limitations.
Full Debug and ASan/UBSan CTest both pass 9/9: 578 unit cases (two expected opt-in skips),
64 publication-fault cases and 87 CLI integration cases, plus the other CTest runners. Twenty
focused batches pass 1,780 unit, 300 fault and 260 PTY/lifecycle method runs. Pinned lint,
Semgrep, runner checks, offline dependency verification and analyzer detection self-tests pass.
[Ubuntu CI on the production revision](https://github.com/hbmartin/codet-compiled-agent/actions/runs/40589394974)
passes compilation, all-source analysis, actual Unicode coverage, CTest, lint and Semgrep.

Local Cppcheck completes 25/25 with zero findings. Local Clang-Tidy completes 24/25 with the
same five source warnings and one SDK13-incomplete translation-unit error as baseline. Clang
Analyzer completes 25/25 with the unchanged `context.cpp` division warning. Diagnostic multisets
and final input hashes were checked; no suppressions or checks were weakened.

The isolated Release comparison uses identical settings/harnesses, one warm-up and five alternating
measured runs, repeated as a second set without concurrent local tests or analyzers (408 total
samples). Times below are median milliseconds at the helper/harness boundary, not end-to-end
interactive terminal latency. No timing-based CI threshold or general output speedup is claimed.

| Workload | Baseline, sets 1 / 2 | Changed, sets 1 / 2 |
| --- | --- | --- |
| Incremental 1 KiB input | 9.224 / 9.511 | 0.164 / 0.169 |
| Incremental 8 KiB input | 579.990 / 581.164 | 1.263 / 1.233 |
| Incremental 64 KiB input | 37283.061 / 37362.509 | 9.792 / 9.889 |
| 256 unchanged 64 KiB redraws | 289.230 / 289.969 | 0.020 / 0.018 |
| Thirty 2 MiB history entries | 11.693 / 11.823 | 0.414 / 0.416 |
| diagnostics-amplified | 385.617 / 380.135 | 386.069 / 382.525 |
| diagnostics-standalone-amplified | 385.747 / 386.986 | 384.910 / 387.264 |
| diagnostics-pipe-amplified | 377.606 / 389.200 | 375.796 / 393.263 |
| editor | 80.042 / 80.256 | 80.402 / 79.760 |
| export-small-file | 51.085 / 52.218 | 52.069 / 53.155 |
| export-large-file | 267.009 / 276.607 | 265.291 / 276.332 |

The 64 KiB incremental workload prepares 131,071 bytes instead of 2,147,516,416; cumulative
C++ allocation bytes fall from 8,592,750,241 to 7,206,468. Unchanged redraws prepare zero new
input bytes; the 256 viewport strings allocate 20,480 bytes in total. The large-history workload
retains 262,144 rendered payload bytes instead of 62,914,590. Input checksums match, and
allocation/write/stat/flag/pipe-bound positive controls succeed in every sample. Diagnostics and
exports have identical allocation/byte and observed native-operation counts. Small-file exports
are about 1.9%/1.8% slower; large-file exports about 0.6%/0.1% faster. Counters cover C++ new and
directly observed native calls, not all libc/libedit allocation or I/O.

Earlier failures are retained separately from final passing checks. One case-insensitive filesystem
test failed during initial sanitizers and reproduced 2/100 times on exact baseline; changed focused
sanitizers passed 20/20 without weakening assertions. One intermediate native child-setup failure
was not reproduced in 100 baseline or 100 changed focused reruns; its cause remains unknown.
The worker-completion prefix loss and new development-time static findings were fixed before the
final suites and measurements. Native Tiger runtime/cross-build tools and Intel macOS runtime
were unavailable. Tiger's pinned unsuffixed wide-curses configuration is preserved, not reported
as executed validation. Blocking-TTY cancellation and the absence of a visually-unique-Unicode
or exactly-once-output guarantee remain documented limitations.


## Explicit discard prompts and recoverable redraw follow-up

Fetched baseline `a850f04` contains PR #41. Validated/measured production revision
`28530968692d77848e8f1ffb133d1682ca22f977` makes stale line ownership explicit without
releasing the expired-answer guard. A deterministic gated PTY control fails on baseline in both
line and curses modes because a fresh-looking approval prompt appears while an old draft still
owns the line. With the change, `discard>` appears first; Enter only discards that line, and a
subsequent fresh answer is required. Approval and question expiry, ordinary drafts across worker
completion, request matching, quit/EOF/cancellation, resizing and page navigation retain their rules.
One classifier handles stale/quit/answer/message decisions; opening approval parses identity once.

Ordinary native row/refresh errors now return status, preserve history/input and the worker, and
leave the display dirty until the next normal UI event. No retry spin or automatic line-mode switch
is added. Approval rendering still requires successful complete rows and refresh before credit.
The identical native negative-control harness throws on baseline (exit 31) and returns status on
the change (exit 0). Real CLI tests inject clear, put and refresh failures: ordinary work and partial
input survive; equivalent approval failure cancels without running the tool. Preparation/allocation
exceptions remain exceptional, and cleanup retains the existing original-exception rules.

Ordinary transcript preparation now bounds raw human encoding and locale fallback before making
full-message display copies. A 512-byte stack chunk and complete token boundaries preserve prose,
literal/escape distinctions, CRLF policy and supported Unicode. The omission rule is shared with
history, including its bytes within the 256 KiB payload / 1,000-line cap. Pending review documents
remain complete and independent; approval excerpts remain capped at 16 KiB. Character deletion
mutates the cached UTF-8 tail once, retaining previous valid/partial/malformed byte semantics.
The uncached input reference and real-cell tests of the cached viewport remain in place.

[Commands, hashes, controls, findings and limits](validation/approval-ui-recovery-followup.json),
[final medians](validation/approval-ui-recovery-followup-performance.csv), and
[every final warm-up/measured sample](validation/approval-ui-recovery-followup-performance-samples.csv)
record the verification. Full Debug and ASan/UBSan CTest pass 9/9 each: 582 unit cases (two expected
opt-in skips), 65 publication-fault cases and 90 CLI integration methods, plus the other runners.
Twenty focused batches pass 1,860 unit cases, 320 fault cases and 320 PTY/lifecycle methods.
Pinned lint, Semgrep, runner checks, offline dependency verification and analyzer canaries pass.
Ubuntu required checks pass on the exact implementation revision; the run is linked in the JSON.

Local Cppcheck completes 25/25 with no findings. Local Clang-Tidy completes 24/25 with exactly
five baseline source warnings plus the existing SDK13-incomplete platform translation unit;
Clang Analyzer completes 25/25 with the unchanged context.cpp division warning. Both local
Clang tools retain nonzero exits; they are not claimed clean. Final input hashes and diagnostic
multisets match the recorded comparison. No checks or suppressions were weakened.

Two isolated Release sets use identical compiler/settings/harnesses, one warm-up plus five
alternating measured runs per workload/variant (552 samples across 23 workloads). Times below
are median milliseconds at the measured boundary, not complete interactive latency. No
timing-based CI threshold or general output performance improvement is claimed.

| Workload | Baseline, sets 1 / 2 | Changed, sets 1 / 2 |
| --- | --- | --- |
| presentation-small | 5.364 / 5.805 | 4.816 / 4.953 |
| presentation-large | 400.069 / 535.025 | 11.476 / 9.363 |
| presentation-controls | 417.076 / 462.803 | 3.442 / 2.797 |
| presentation-unicode | 401.246 / 422.347 | 3.519 / 3.477 |
| presentation-rendered | 230.044 / 247.920 | 7.768 / 7.076 |
| erase-emoji | 0.982 / 1.096 | 0.045 / 0.047 |
| input-65536 | 11.328 / 11.083 | 11.537 / 11.058 |
| input-redraw-65536 | 0.019 / 0.018 | 0.022 / 0.020 |
| diagnostics-amplified | 546.825 / 518.861 | 495.416 / 638.363 |
| diagnostics-standalone-amplified | 496.061 / 552.921 | 503.616 / 538.872 |
| diagnostics-pipe-amplified | 620.393 / 842.987 | 632.292 / 729.535 |
| editor | 153.224 / 119.415 | 151.702 / 124.546 |
| export-small-file | 112.104 / 101.470 | 104.671 / 103.909 |
| export-large-file | 327.281 / 337.087 | 490.522 / 382.320 |

The first correctness comparison exposed repeatable small-message overhead. Its
[medians](validation/approval-ui-recovery-followup-first-performance.csv) and
[all samples](validation/approval-ui-recovery-followup-first-performance-samples.csv) are retained.
The final implementation restores ASCII fast paths, avoids duplicate tokenization and encodes
raw chunks on the stack. Allocation and write/stat/flag/bound-query positive controls pass.
Input and nontruncated presentation checksums match; truncated Unicode can stop earlier at a
complete prepared scalar. Diagnostic/export allocation/byte and observed native-operation counts
are unchanged. Native output timings vary between sets; no export/editor speedup is inferred. The primary large
export sets show higher changed medians (327.281/490.522 and 337.087/382.320 ms baseline/changed).
Two additional isolated alternating sets give 371.178/395.895 and 300.908/295.427 ms; every sample
is embedded in the JSON. Counts are identical, but the timing variation remains unresolved; a
blanket timing no-regression claim is not made.

For 4,096 emoji deletions, cache mutations fall from 16,384 to 4,096 and retained-tail preparation
falls from 24,576 source bytes to zero, with zero C++ allocations in either version. Unchanged
redraws still prepare zero input bytes and copy only their viewport. Presentation work stops at
retained output plus fixed lookahead, including exhausted/small budgets; allocation-failure tests
retain the exception. Counters cover C++ new and directly observed calls, not every libc/libedit
operation or peak RSS. The JSON records allocations, bytes and raw consumed/prepared-byte counts.

An earlier sanitizer full suite aborted in unchanged
`Followup.SpoolFailureKeepsExitStatusAndSerializablePreview`: it reported an unexpected tool error,
then indexed absent metadata. It emitted no sanitizer memory-error report. One hundred exact
baseline and one hundred changed focused runs did not reproduce it; the cause remains unresolved.
Final complete suites and repetitions pass without changing that test or process/spool production
code. Development-time enum-size and baseline-harness build warnings were corrected in source.

Native Tiger runtime, PPC compiler/10.4u SDK and Intel macOS runtime are unavailable; no passing
Tiger build/runtime is claimed. Existing Tiger library selection, ASCII fallback, strict approval
Unicode policy, raw execution/answer/data bytes, storage/journal formats, completed-turn authority,
terminal cleanup ownership and live-to-cleanup-to-file replay ordering remain unchanged.
Blocking-TTY cancellation and best-effort standalone/emergency output retain their existing limits.


## Request prompts and exact history budgets

Fetched baseline `daabed948d3308d393037bcc7b5f85d4c6261603` contains PR #42.
Validated/measured production revision `3ef24f30f4f993e9b88a2b65bf64cef983f0ad12` adds one
controller for cached input, draft/request identity, request kind and prompt presentation.
Question guidance returns after discard in curses; line prompts return after output batches
without a trailing newline or insertion into keyboard echo. Acknowledgement is neutral and restores
the current question, approval or message prompt. Explicit editing of a live draft to empty releases
its intent; presentation clears and stale drafts retain ownership through Enter. Fresh input cannot
answer an expired request or become an unintended message.

Preparation and history insertion share exact rendered-row byte charges. Fitting messages stay
whole; overflow alone reserves the complete omission marker, and a retained LF is reused instead
of adding a blank row. Tiny budgets retain fitting text but no truncated excerpt when the marker
cannot fit. Limits remain 256 KiB/1,000 lines, 16 KiB per approval excerpt and 65,536 raw input bytes;
the complete pending review is independent. Validated raw UTF-8 deletion removes one scalar,
one valid trailing prefix or one malformed byte and updates the cache tail once. Literal escape
spelling remains ordinary editable input.

Native drawing tests assert return status and connect review credit to successful rows/refresh.
Temporary row-status and refresh-check mutants fail the strengthened tests; the control passes.
Identical boundary probes fail against baseline and pass on the change for fitting-message
retention and stray-continuation deletion. Ordinary redraws still wait for the next UI event,
approval remains fail-closed, and scratch-pad acquisition failure retains ASCII fallback.

Final Debug and ASan/UBSan CTest pass 9/9 each: 588 unit cases (two expected opt-in skips),
65 publication-fault cases and 91 CLI integration methods. Twenty focused sanitizer batches cover
99 unit cases, 16 fault cases and 17 PTY methods each: 2,640 runs plus subtests. Pinned lint,
Semgrep, runner checks, Cppcheck and analyzer self-tests pass. Local Clang-Tidy retains exactly
five source warnings plus one SDK13-incomplete platform translation unit; Clang Analyzer retains
the baseline division warning. Their nonzero results are recorded, with no weakened checks.
The required Ubuntu workflow passes the build, all-source analysis and actual UTF-8/wide-curses
host suite. Native Tiger/PPC SDK/compiler and native Intel runtime are unavailable locally.

Identical Release harnesses use one warm-up and five alternating measured runs, repeated twice,
without concurrent local tests/analyzers. Median milliseconds (set 1 / set 2):

| Workload | Baseline | Changed |
| --- | ---: | ---: |
| presentation-small | 4.677 / 4.726 | 4.672 / 4.748 |
| presentation-large | 8.982 / 9.319 | 9.849 / 9.955 |
| presentation-controls | 2.759 / 2.944 | 3.174 / 3.284 |
| history-large | 0.436 / 0.414 | 0.446 / 0.429 |
| erase-emoji | 0.042 / 0.042 | 0.063 / 0.064 |
| input-65536 | 9.900 / 10.052 | 9.928 / 10.293 |
| input-redraw-65536 | 0.017 / 0.018 | 0.018 / 0.018 |
| export-large-file | 286.780 / 264.576 | 275.968 / 263.757 |

The first correctness comparison exposed a repeated prefix scan during truncation. The final shared
scan also returns its previous token boundary, eliminating that second traversal; both initial
measurement sets remain published. Exact accounting still has a bounded trimming pass, and
validated deletion adds a bounded raw-token check. The large ASCII/control presentation samples
remain approximately 7–15% slower than baseline; 4,096 validated emoji deletions take about
0.063 ms versus 0.042 ms. No general latency improvement is claimed.
Diagnostics and exports retain identical allocation and directly observed native-operation counts.
Unchanged redraws prepare zero input bytes; valid emoji deletion still uses one mutation and no
retained-tail preparation or allocation. History preparation stays bounded by retained output plus
fixed lookahead. Some Unicode row-allocation counts change because corrected retention keeps a
different prefix. Counters are C++ allocation/direct-call observations, not peak RSS or complete
libc/libedit activity; the input harness measures cached boundaries, not end-to-end keystroke latency.

[Commands, hashes, negative controls, baseline findings and limitations](validation/request-prompt-followup.json),
[final medians](validation/request-prompt-followup-performance.csv),
[final samples](validation/request-prompt-followup-performance-samples.csv),
[first correctness medians](validation/request-prompt-followup-first-performance.csv), and
[first correctness samples](validation/request-prompt-followup-first-performance-samples.csv)
retain all evidence. Initial PTY/lint development failures are recorded. One later repetition observed the approval
marker after creation but before its content was ready. The final test waits for turn completion
before checking the same exact bytes; all 17 PTY methods then pass twenty fresh repetitions.
Production source is unchanged by that test-readiness correction; the failed log is retained.


## Coherent line prompts and production-budget coverage

Fetched baseline `a42be250` contains PR #43. Validated source `bb2f593e0a3098ad9a996365e100b401d546ecf9` coordinates
line prompts, surviving drafts and native terminal-bound display/diagnostic output. A batch separates
the echoed line before output, then restores the prompt plus complete draft once. Active turns show
requests, discard acknowledgement or a live draft; idle empty prompts belong to libedit. Completed
expired answers receive an expiry explanation, while stale-line acknowledgement remains distinct.

The original PTY probe printed `hefixture done`, then a blank prompt while retaining `he`. Backspace
plus `llo` submitted `hllo`. The fixed probe shows `he` after restoration; plain messages no longer
leave abandoned steering prompts. Native PTYs verify warning/file-notice batches and recovery
reporting preserve approval/question drafts, original exception identity and cancellation reset.

Full Debug and ASan/UBSan CTest pass 9/9 each: 593 unit cases, 65 fault cases and 93 integration
methods. Twenty focused sanitizer batches cover 104 unit cases, 16 fault cases and 19 PTY methods
each (2,780 runs plus subtests). Pinned lint, Semgrep, runners, Cppcheck and analyzer self-tests
pass. Local Clang findings exactly match the recorded baseline; the required Ubuntu workflow
passes wide-curses/UTF-8 tests and all-source analysis on the final source/test revision.

Production-budget tests and identical Release harnesses now use 262144 bytes, including row
charges, exact fits, newlines, Unicode/escape boundaries, omission markers and allocation failure.
One warm-up plus five alternating runs, repeated twice without concurrent task tests/analyzers,
compare baseline, correctness, lazy-width and omitted-trim variants. Median milliseconds
(set 1 / set 2):

| Workload | Correctness | Retained width change | Omitted trim candidate |
| --- | ---: | ---: | ---: |
| input-narrow | 1.940 / 1.916 | 1.642 / 1.611 | 1.958 / 1.609 |
| approval-invisible | 2.381 / 2.466 | 2.165 / 2.247 | 2.167 / 2.269 |
| presentation-small | 5.056 / 4.854 | 5.179 / 5.284 | 5.019 / 5.309 |
| presentation-large | 10.356 / 10.489 | 10.432 / 12.914 | 10.061 / 10.435 |
| presentation-controls | 3.256 / 3.200 | 3.466 / 3.923 | 3.072 / 3.079 |
| export-small-file | 65.607 / 63.337 | 62.229 / 58.149 | 66.421 / 61.244 |

Lazy rejection eliminates 100,000 locale conversions in each targeted workload without adding
allocations. The first trim tracker slowed small presentations; its guaranteed-fit refinement
improved large presentation modes but small-file exports regressed in both sets. The trimming
optimization is omitted; all candidate samples remain published. Diagnostics and exports retain
equal observed allocation/native-operation counts. These are cached-boundary/direct-call
measurements, not complete libc activity or end-to-end editor latency; no general speedup is claimed.

An initial full run exposed the idle discard handoff bug; returning to libedit fixed the existing
expiry assertions. Pinned lint also caught unbound PTY loop callbacks, which are now bound explicitly.
Strict Release baseline warnings remain documented; identical benchmark-only compiler settings
were used throughout. Required checks were not weakened. Native Tiger/PPC SDK/compiler and Intel
runtime coverage are unavailable. The final one-line idle handoff correction is outside the measured
paths and is covered by final full suites, repetitions and Ubuntu CI.

[Commands, hashes, baseline findings, CI and limitations](validation/line-prompt-followup.json),
[final medians](validation/line-prompt-followup-performance.csv),
[final samples](validation/line-prompt-followup-performance-samples.csv),
[first candidate medians](validation/line-prompt-followup-first-performance.csv),
[first candidate samples](validation/line-prompt-followup-first-performance-samples.csv),
[initial full-suite failure](validation/line-prompt-followup-initial-debug-failure.log),
[initial lint failure](validation/line-prompt-followup-initial-lint-failure.log), and
[strict Release baseline limitation](validation/line-prompt-followup-strict-release-failure.log)
retain the evidence.


## Safe bounded line input and context-aware diagnostics

Fetched baseline `60b8c4e` contains merged PR #44. Validated application/test source `6f29048468561b45636fc10e503ade435737e0d0`
uses strict cached presentation for active line echo, deletion and restoration. Editable rows reserve
the final stdout column and cap their tail at 256 columns; unknown or sub-ten-column widths use
labelled snapshots. Complete raw input and pending approval requests are retained. The controller
owns invalidation and rejection reasons, and exactly empty ordinary Enter never queues a message.

Native PTYs cover C/UTF-8 input, wide and escaped combining characters, malformed bytes, narrow/basic
terminals, separate input/output terminals, `/dev/tty`, redirected stderr and stdout replacement.
An old destination retains only a separator boundary; diagnostics there cannot repaint the new row.
Recovery tests close the request and release repaint ownership before draining, preserving original
exception identity. Fixed worker warnings coalesce without allocation and render on the main thread.

Darwin reports POLLNVAL for a valid `/dev/tty` alias. An isolated probe confirms zero-byte reads
after that result; the bounded select fallback fixes both active and libedit input readiness.
Keyboard row repaint remains best effort. A negative control proved that using checked display for
every keystroke incorrectly latched failure; the regression now protects the existing exit policy.

Full Debug and ASan/UBSan CTest pass 9/9 each (605 unit cases, 67 fault cases and 98 integration
methods), with the two existing network/Tiger opt-in skips. Final focused unit and PTY batches pass
twenty repetitions. Pinned lint, Semgrep, runner checks, Cppcheck and analyzer self-tests pass.
Local Clang findings exactly match the existing baseline; strict required Ubuntu analysis and tests
pass. No checks were weakened. One existing sanitizer startup-signal race is retained with its
passing rerun, twenty further socket/Unicode repetitions, and passing final quiet suites.

Identical Release harnesses use one warm-up plus five alternating measured runs, repeated twice
without concurrent task tests/analyzers. Median milliseconds (set 1 / set 2):

| Workload | Fetched main | Changed |
| --- | ---: | ---: |
| line/native-busy | 13.038 / 11.488 | 0.967 / 0.851 |
| line/busy | 0.056 / 0.056 | 0.101 / 0.101 |
| line/editing | 2.084 / 2.130 | 3.428 / 3.272 |
| line/deletion | 0.022 / 0.022 | 0.084 / 0.087 |
| line/unchanged | 0.022 / 0.022 | 0.028 / 0.028 |
| output/diagnostics-amplified | 393.452 / 397.294 | 396.674 / 389.585 |
| export/large | 278.021 / 281.400 | 278.091 / 281.338 |

A 20 KiB draft across 100 native output batches falls from 2,049,700 to 8,900 emitted bytes
(99.6% less), with zero repaint allocations/preparation. Both versions make 300 direct output
operations and 300 fstat calls in that workload; the change uses 200 rather than 300 fcntl calls
and adds 100 geometry queries. Unchanged line-prompt redraws perform no output, geometry query, allocation or
input preparation. Two fixed row buffers use 1,536 bytes; the arm64 line owner grows from 200 to
1,936 bytes including destination state. Full input remains capped at 65,536 raw bytes.

Correct safe row rewriting increases editing/deletion traffic compared with raw byte echo and
one-column backspace. Injected-sink and export timings can be slower; no general speedup is claimed.
Diagnostics/exports retain their observed allocation/native-operation counts. The library-internal
editor writes are not fully observable by these interposers; zero direct counts do not mean no I/O.
No timing thresholds are added. Native Tiger/PPC SDK/compiler and Intel runtime checks remain
unavailable; Tiger-compatible CR/space rendering, ASCII fallback and existing native libraries are
retained. Blocking TTY output and irrecoverable emergency delivery retain their documented limits.

[Commands, hashes, results, CI and limitations](validation/line-input-rendering-followup.json),
[all measured medians](validation/line-input-rendering-followup-performance.csv),
[all final samples](validation/line-input-rendering-followup-performance-samples.csv),
[baseline reproductions](validation/line-input-rendering-followup-reproductions.json), and
[development failures and negative control](validation/line-input-rendering-followup-development-failures.log)
retain the evidence. Historical measurements are unchanged.
