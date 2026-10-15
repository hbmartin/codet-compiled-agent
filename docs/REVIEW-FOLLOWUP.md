# Follow-up review dispositions

Base: main `1d6c0e3bfe0ca247b421bdc54e72a13fbc254e2d` (merged PR #4). This batch preserves C++17, PPC750/Tiger, application dependency pins, application exit codes, the JSONL subset and unrelated local work. No PR changes or push are part of delivery.

## Application review

| Item | Disposition | Implementation / regression evidence |
|---|---|---|
| A1 Search authorization | Intentional behavior | Bash retains command approval, headless denial and plan restrictions. Quoting cannot evade scoped grants. No search exemption or revived Grep. Portable approved search is documented and tested. |
| A2 Short startup timeout | Already fixed; strengthened | Absolute command deadline plus five-second setup safety bound; status 124 on expiry, deterministic readiness tests, no execution after budget expiry. A fixed independent wait is not adopted. |
| A3 Fresh provider/model mismatch | Fixed | One resolver for fresh/resume/fork with configured-pair provenance and field-specific overrides. Selection matrices in Followup and CLI integration. |
| A4 SessionEnd after /new | Already fixed; retained | Started session ID captured by value; both terminal modes test one invocation-level lifecycle pair. |
| A5 Darwin one-shot exit | Fixed | Latched observation survives signal/reap exceptions; owned cleanup retries, confirmed ownership loss disables signaling. Deterministic one-shot and ownership-loss tests. |
| A6 Search output/exclusions | Fixed/documented | Private bounded disk capture, head/tail preview, retention metadata and paging; portable find/grep exclusions tested. Shell search requires explicit exclusions. |
| A7 Matching top-level model | Fixed | Provider-only change consults matching configured pair before nested provider model and built-in fallback; selecting saved provider retains saved model. |
| A8 Empty Codex/Anthropic tools | Fixed | Empty optional tools omitted by every adapter, nonempty declarations retained. Actual-adapter compaction fixtures. Compatibility hardening, not a verified live API failure. |
| A9 Help/version precedence | Fixed | Parser records errors while respecting consumed values and literal `--`; help wins, then version. CLI regressions. |
| A10 Inherited signals/startup interruption | Fixed | Reset child dispositions including SIGHUP and clear mask; retry EINTR and check cancellation before setup/release errors. Real signal and injected handshake tests. |
| A11 Cleanup diagnostics | Fixed | Include exception details, buffer hook diagnostics, restore curses, then print stderr without changing invocation status. PTY ordering/restoration regression. |
| A12 Stale responding status | Fixed | Clear progress on completed/failed events, including interruption; resize/repaint tests verify mode remains and progress disappears. |
| A13 Temporary fixture duplication | Fixed | Shared checked RAII TempDirectory; cleanup cannot throw. |
| A14 Dead model_provider fallback | Fixed | Normalize alias once per layer, provider authoritative; remove eager fallback. Conflicting alias regression. |
| A15 Handshake channel count | Deferred optional cleanup | Keep existing pipe/socketpair protocol for this correctness batch. |

## Analysis / repository review

| Item | Disposition | Implementation / regression evidence |
|---|---|---|
| B1 Suppressed parser abandonment | Fixed | Checksum-pinned Cppcheck 2.22.0 built-in parser with safety mode; fatal safety evidence cannot be hidden by inline suppression. Real dependency-header canaries. Earlier 2.13 C++ coverage evidence is invalid. |
| B2 Progress counted as coverage | Fixed | Per-TU XML, successful completion, semantic checker report and cache artifacts plus defective canaries; premature progress/empty-artifact tests. |
| B3 Missing internal failure IDs | Fixed | Parser/internal/AST/instantiation/limit failures gate at every location, including dependency headers. Table-driven regression. |
| B4 Vacuous toml++ audit | Fixed | Normalize split/attached/relative/symlink/quoted includes; count replacements and compare actual compiler preprocessing for affected TUs; empty/mismatched audits fail. |
| B5 /var path attribution | Fixed | Resolve both source and repository root; alias-root tests. |
| B6 Broad analysis ignore | Fixed | Generated reports stay under ignored build output; analysis source directories are no longer globally ignored. Unrelated local analysis files are not imported. |
| B7 Developer image dependencies | Fixed | Clang/Tidy 18, pinned Cppcheck, Python/PyYAML; explicit network provisioning and offline analysis. Root-context image instructions. |
| B8 Duplicate CI runs | Fixed | Pull requests plus pushes to main/manual dispatch; feature pushes no longer duplicate PR analysis. |
| B9 Unnecessary analysis builds | Fixed | Configure-only analysis targets; explicit application/test build stage retained. Dedicated Clang cache leaves GCC builds untouched. |
| B10 Ctrl-C orphan analyzers | Fixed | Cancel queued work and terminate owned groups; actual SIGINT worker/queue regression, exit 130. |
| B11 Timeout cleanup races | Fixed | Disappearing groups handled, partial output preserved, TERM/KILL/drain/reap bounded; inherited-pipe regression. |
| B12 Missing explicit binary fallback | Fixed | Explicit binary selections fail clearly rather than selecting another tool. |
| B13 argparse program name | Fixed | Use description keyword; normal usage/help. |
| B14 Library-specific completeness workaround | Fixed structurally; necessary adapter retained | General semantic completion/fatal/canary checks supplement a verified generated toml++ token-paste adapter. Original headers remain intact. |
| B15 Inconsistent dependency attribution | Fixed | Shared resolved-path policy across analyzers; primary or execution/call locations gate, import identity alone does not. Dependency-only raw diagnostics retained. |

## Validation and remaining target boundary

See `MODERN-ANALYSIS.md` for reproducible commands and report locations. Reports record first-party source/header hashes, tool versions, commands, raw diagnostics, compatibility audit and per-TU coverage proofs. Raw dependency findings remain available even when excluded from the application gate. Analyzer fixtures never link or run intentionally defective code. Tests use fake providers and local subprocesses; no paid requests or real credentials.

The only new local analyzer suppression is `performance-no-int-to-ptr` on SQLite's required `SQLITE_TRANSIENT` destructor sentinel in `Statement::text`; retaining the sentinel copies the temporary string and avoids a dangling SQLite binding. No global suppression or warning baseline is added.

Source-hashed analysis evidence is checked in at [validation/followup-analysis.json](validation/followup-analysis.json); full raw reports stay in ignored build artifacts. Final host, analyzer, image and PPC results are recorded below before the local delivery commit. Actual Tiger execution, subscription round trips and G4/512 MB startup/peak-memory acceptance remain pending owner-supplied target results. Cross-build/static checks do not mark runtime acceptance complete.


### Completed host/static validation (2026-10-15)

- macOS: CTest unit/CLI/integration suites pass, including 110 C++ cases (109 passed; the opt-in live TLS case skipped) and 26 loopback/PTY CLI cases. ASan/UBSan suites pass. Analyzer runner: 33 tests pass.
- Linux developer image: the same host and ASan/UBSan suites pass. All three analyzers pass on all 12 application translation units with zero gated findings; synthetic detection self-checks and all three real-header Cppcheck canaries pass. Dependency-only diagnostics remain in raw reports.
- macOS Cppcheck 2.22.0 and Clang Static Analyzer also pass all 12 translation units. C++ semantic reports activate 125 checkers; the C helper activates 81. Semantic completion artifacts and canaries, rather than those counts, establish coverage.
- Dedicated analysis targets produced no application/test executable. Reconfiguring the Clang analysis database preserved the Linux GCC host-cache hash. No runtime dependency pin changed.
- PPC GCC 14.2 cross-build and static verification pass: PPC32 big-endian/PPC750 Mach-O, expected TigerPorts/system dependencies, no container library paths or LC_RPATH. The regenerated acceptance archive is `build/dist/codet-tiger-ppc.tar.gz`; PPC executables were not run.
- The complete developer image builds from a fresh local clone using the documented root build context and explicit provisioning stage. Dependency sources were reconstructed from checksum-verified cached archives. Tool version/PyYAML checks and all three analyzer detection self-checks then pass with networking disabled; configure-only targets create no application/test executable.

Reproduction: `make test`, `make test-sanitize`, `make test-modern-analysis-runner`, `make analyze-modern-selftest`, `make analyze-modern`, `make verify-tiger`, and `make tiger-package`. On macOS the native analyzer runs use `build/analysis-venv/bin/python` (PyYAML installed); Clang-Tidy is exercised in the Linux image. Full logs remain under ignored `build/followup-*` and analyzer report directories. GitHub-hosted CI was not dispatched because this delivery is local only.
