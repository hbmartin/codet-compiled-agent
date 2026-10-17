# Static analysis findings for the recovered PPC application

Historical analysis of the recovered C target at the revision below. Its local `analysis/` harness and Makefile targets have been retired; the reproduction commands describe that archived setup. Current application checks are documented in [MODERN-ANALYSIS.md](MODERN-ANALYSIS.md).

The highest-priority issues are the reconstructed address calculation in `cache_init`, which adds two object addresses before initializing a dynamic string, and unsafe shell quoting in search operations. Tool execution reaches the cache initializer; query/regex text can alter generated shell syntax under the prerequisites below. The reviewed results distinguish source-confirmed problems, conditional leaks, and model-related diagnostics.

Analyzed application revision: `37bcaaf48512c2948487e783bcb59180dcc2c388`, on October 14, 2026 (America/Los_Angeles). Application C and headers are unchanged by this analysis setup. No PPC application or defective fixture was executed.

## Reproduction and coverage

```sh
make analysis-image
make analyze-selftest
make analyze
# Or run independently:
make analyze-gcc
make analyze-cppcheck
```

`make analyze` intentionally returns nonzero on this baseline because GCC reports analysis limits; its recipe still runs Cppcheck. Findings alone do not cause failure. Compile errors, preprocessing errors, malformed diagnostic output, crashes, timeouts, and reported limits are not counted as completed coverage. `ANALYSIS_TIMEOUT` defaults to 300 seconds per command. Images are built with networking; actual scans have no network and mount source read-only.

The source manifest comes directly from `APP_SOURCES`: 32 application subsystem files, `src/storage.c`, and `src/text_compat.c`. Historical ABI/startup/storage stubs and `src/subset.c` are excluded. Compiler flags, source SHA-256 hashes, target macros, per-command logs, diagnostics, and summaries are retained under ignored `build/analysis/`. Rebuilding the analysis image can resolve newer Ubuntu packages; each run records the actual analyzer versions.

| Analyzer | Version | Application coverage | Raw results |
| --- | --- | --- | --- |
| GCC analyzer | GCC 14.2.0, pinned production cross compiler | 34/34 compiled; 8 without reported limits, 26 with limits; no compiler failures | 16 candidate warnings, 1,296 complexity warnings, 345 notes |
| Cppcheck | 2.13.0 | 34/34 processed; no reported limits or parser failures | 101 diagnostics: 25 errors, 26 warnings, 50 portability observations |

GCC uses object compilation at `-O0 -fanalyzer -fno-strict-aliasing` with the production target/SDK flags and both complexity warnings enabled. Cppcheck uses GCC's production-configuration (`-O2`) preprocessed C, C11, exhaustive checks, and warning/performance/portability checks. Source locations survive preprocessing. Its platform XML derives sizes and char signedness from the real cross compiler: 32-bit int/long/pointers/size_t/wchar_t, 64-bit double, 128-bit long double. This configures type widths; it does not give Cppcheck a complete model of Darwin libraries or the reconstructed object layout.

The temporary selftest confirms GCC reports a double-free and Cppcheck reports an out-of-bounds array index. `make verify` passed with the production `-Wall -Wextra -Wformat=2 -Werror` flags, SDK layout checks, 365 expected functions, and PPC32/G3 Mach-O checks. Analysis objects do not replace production objects.

## Prioritized findings

### P1 Cache initialization constructs a destination by adding object addresses

**High confidence, source-confirmed during triage.** `src/subsystems/tools.c:998-1000` passes `ds_init` a pointer formed from `_g_state.history.entries[0]` plus the integer-converted address of an entry in `app->history`, plus offsets. That is not an offset relative to either allocation. `ds_init` writes a pointer and two words through this computed destination. The intended cache string resides at `&app->cache[i].result`, whose layout is explicitly defined in `include/recovered-types.h:484`.

Trigger: `tool_execute` calls `cache_init` at `tools.c:1607`; `worker_child_main` also calls it for a newly allocated application state at `workers.c:1967`. Impact: writes to the wrong part of application state, corrupted history, and potentially invalid frees when clearing a populated cache. The same address calculation appears in `cache_clear` (`tools.c:1100`), where it is passed to `ds_free`, and related expressions occur in failure counters (`1163-1166`) and TUI plan updates (`terminal_ui.c:4072-4079`).

The current linked symbol map places `_g_state` at `0x45490`, whereas the original recovery address was `0x30c9a0`. Using those constants and the asserted structures, the initializer computes `app + 0x6090ac + i*24` instead of `app + 0x8d05bc + i*24`: it writes into the history region, `0x2c7510` bytes before the intended cache result. The expression happens to yield the intended offset only with the original global address. This statically confirms a relocation error; it does not establish a measured runtime crash.

Evidence: manual follow-up of Cppcheck's `objectIndex` diagnostics at `tools.c:997`/`1029` and the same recovered cache traversal. Cppcheck does **not** directly diagnose the double-base calculation. Suggested repair: reconstruct initialization around `app->cache[i]` and initialize `&app->cache[i].result`, checking the original instruction evidence before changing behavior.

### P1 Search text can escape single quotes in generated shell commands

**High confidence in unsafe command construction; actual exploitation depends on input control and the existing approval flow.** Flawfinder flags `popen` at `documentation_rag.c:396` and `tools.c:786`; Clang-Tidy also reports `cert-env33-c` at these locations. `fetch_embedding` escapes JSON characters but leaves apostrophes unchanged before placing the query inside a single-quoted shell argument (`documentation_rag.c:360-396`). `tool_grep` likewise inserts the regex and working directory directly inside single quotes (`tools.c:782-786`). These are already annotated as preserved `SHELL-01` behavior in the recovered source.

Trigger: an apostrophe-containing query reaches embedding search through `rag_search` with a nonempty documentation database (`documentation_rag.c:491`); `/reference` passes the query at `terminal_ui.c:2739`, and the RAG tool passes it at `tools.c:1656`. For grep, the configured tool argument reaches `popen` after the normal outer tool approval prerequisites. Impact: shell command construction can be changed by the text, with the application's account privileges. This is static corroboration of existing behavior, not a demonstrated remote or approval-bypass exploit.

Suggested repair: use the existing libcurl interfaces for the embedding HTTP request and an argument vector or an in-process implementation for grep, so query/regex text never becomes shell syntax. Preserve the intended request payload and tool approval policy.

### P2 Recovered traversals cross the bounds of scalar and array subobjects

**High confidence for the C-language violation; runtime consequences require review.** Cppcheck reports five `objectIndex` errors and sixteen `pointerOutOfBounds` observations. Examples are `tools.c:997,1029` (indexing backward from the scalar `valid` field), `workers.c:1489-1499` (indexing a word pointer taken from a `data` member), and `conversation_state.c:162-163` (advancing from `mcp_name` into later `RecoveredToolCall` objects).

Trigger: cache initialization/lookups, tool-call copying, and related profile/tool traversals. These expressions may reach intended bytes within an enclosing allocation, but they leave the originating C subobject; `-fno-strict-aliasing` does not legalize that pointer arithmetic. Impact: optimization-sensitive behavior and an elevated chance that an incorrect inferred offset becomes an invalid access. No exploitability or actual allocation overrun is established by these diagnostics alone.

Suggested repair: use the recovered typed arrays and fields directly, or compute justified byte offsets from the enclosing object, preserving the verified PPC32 sizes and offsets. Review the P1 calculation separately rather than dismissing every offset expression as decompiler noise.

### P2 Path trimming forms pointers before the start of a token

**High confidence for undefined pointer arithmetic.** `tools.c:197-200` computes `pcVar4 + (uVar5 - 1)` and tests `pcVar4 + (uVar5 - 2) < pcVar4` using unsigned lengths. A token containing only spaces becomes an empty string after leading-space trimming, making `uVar5 == 0`; the subtraction wraps before pointer formation. Even for nonempty tokens, the overflow test itself relies on invalid pointer arithmetic.

Trigger: a whitespace-only component in the comma-separated paths accepted by `tool_read_file` (also called by `tool_read_and_edit`). Impact: undefined behavior in path handling; a concrete out-of-bounds read is not proven by the report. Evidence: Cppcheck `invalidTestForOverflow` at line 200 and source inspection. Suggested repair: reject empty trimmed tokens and express trailing trimming using bounded integer indices before constructing pointers.

### P2 The provider command compares the argument against the prefix terminator

**High confidence, directly identified by Clang-Tidy.** `terminal_ui.c:2660` compares eleven bytes with the ten-character prefix `"/provider "`, thereby including its terminating NUL. Normal documented commands such as `/provider openai` cannot match because their provider argument begins where the literal has its NUL. The branch then copies from `command + 11`, also one byte beyond the intended start of the argument. This is a command-parsing defect; the checker's buffer-overflow wording is not itself proof of an allocation overrun.

Trigger: the TUI's documented `/provider <name>` command (`terminal_ui.c:3565-3567`). Impact: direct provider selection fails; the bare prefix can enter a branch reading beyond its logical terminator. Evidence: `bugprone-not-null-terminated-result` and the literal's length. Suggested repair: derive both comparison length and argument offset from `sizeof("/provider ") - 1`, then handle an empty argument explicitly.

### P3 JSON convenience adders leak a child when the parent is absent

**High confidence under the stated failure condition.** GCC emits five `-Wanalyzer-malloc-leak` warnings in `bundled_cjson.c:1653,1674,1695,1716`. The convenience helpers allocate a child and call `cJSON_AddItemToObject`; that function returns without consuming or deleting the child when the parent or name is null (`1618-1633`). One root cause accounts for the five warnings, including both a string and its node.

Trigger: for example, parent `cJSON_CreateObject()` fails but a subsequent child allocation succeeds; callers such as `worker_serialize_in` do not check the parent first. Impact: leaked allocations under allocation failure or invalid API input. Suggested repair: define and enforce ownership on failed attachment, or validate parent creation before allocating children.

## Candidates and diagnostics not promoted to confirmed defects

- **GCC stack over-read at `terminal_ui.c:4575`: rejected for the current call path.** The bound passed to `strncpy` is larger than the source frame, but `worker_serialize_in` first writes the 256-byte path with `snprintf`; the fixed PID-based path terminates well within that space. `strncpy` stops reading at its terminator. This is a missing cross-file contract, not evidence of an actual 1,023-byte read.
- **Cppcheck null writes in `rag_init` (`documentation_rag.c:97-98`): rejected for its reported callers.** They pass `(NULL, 0)`. The unsigned condition `path_capacity - 1U < 0x3ff` is false for zero, so neither write executes. A hypothetical `(NULL, positive capacity)` violates the helper's effective input contract, but no such call was found.
- **GCC uninitialized reads: seven reports, not confirmed.** The cited CLI/configuration buffers follow successful `fgets`; the model-cache timestamp follows successful `fread`. The diagnostic paths do not model these writes through the reconstructed byte-frame views reliably.
- **Cppcheck uninitialized byte-frame reports: twenty, unresolved model candidates.** Examples follow `snprintf`, `stat`, string helpers, and initialization calls. Treat them as requiring per-function contracts, not twenty proven uninitialized reads.
- **GCC MCP descriptor leaks: two, not confirmed.** Pipe ends retained in the server are intentional, and the second-pipe/fork failure branches close previously opened descriptors. The reports are insufficient to establish a lost descriptor on the actual initialization path.
- **GCC null argument in `http_buf_append` (`http.c:88`): unresolved overflow/model candidate.** It follows a zero-capacity buffer and a branch skipping growth. Ordinary nonempty SSE lines allocate first; reaching that branch needs overflow or inconsistent state. The helper lacks explicit arithmetic overflow guards, but the analyzer path alone does not establish a reachable remote overflow before allocation/growth limits intervene.
- **Cppcheck redundant null checks:** the Anthropic JSON branches guard objects and strings before use; configuration profile pointers derive from an embedded array. The Ollama URL diagnostic requires a null return from `config_get_url`, whose normal configuration paths provide embedded strings. None is promoted solely from a redundant-check warning.
- **Cppcheck scanf warning (`platform.c:37`): rejected.** `local_28` is an `int[6]` view of the frame and decays to `int *`; Cppcheck misclassifies the macro expression as scalar.
- **Remaining portability and style observations:** the 34 pointer/integer-conversion observations reflect explicit PPC32 recovery representations; the eight printf signedness warnings need format/ABI review rather than a severity inferred from their count. Three negative-to-unsigned calculations and three repeated/opposite conditions are retained in raw output; some express reconstructed wraparound or goto control flow. They are lower priority than the memory/address findings above.

## Additional tool evaluation

```sh
make analysis-extra-image
make analyze-extra
# Repeat just one evaluation, if needed:
make analyze-extra ANALYSIS_EXTRA_TOOL=clang-tidy
make analyze-extra ANALYSIS_EXTRA_TOOL=compatibility
```

The extra image installs free tools separately from the production/primary images. It records package versions in `build/analysis/analysis-installed-packages.txt`. The primary source hashes remain unchanged; extra-tool provenance is recorded in `extras-manifest.json`. Optional blockers cause `make analyze-extra` to return nonzero and are recorded in per-tool summaries; they do not change the primary targets.

| Original rank | Tool and evaluated version | Outcome | Additional value and limitation |
| --- | --- | --- | --- |
| 3 | Clang Static Analyzer 18.1.3 | 34/34 units processed; 599 diagnostics, representing 503 unique check/location combinations | Independently corroborates four JSON attachment leaks and the unresolved HTTP null-argument candidate. The remaining 594 reports recommend alternative library APIs, including repeated header observations; they do not establish 594 vulnerabilities. |
| 4 | Clang-Tidy 18.1.3 | 34/34 units processed; 1,233 diagnostics | Finds the provider-prefix defect. Its 500 `clang-analyzer-*` diagnostics all overlap the standalone analyzer by check/location. Most other output concerns ignored return values (270), assignments in conditions (245), narrowing conversions (103), and related recovery conventions. |
| 5 | Flawfinder 2.0.19 | All 34 sources scanned; 637 lexical hits | Corroborates the known search shell-quoting hazards. Levels: 21 at level 4, 21 at level 3, 379 at level 2, 216 at level 1. A level is scanner triage, not the finding's final priority. |
| 6 | PC-lint Plus | Not run; license/tool distribution not supplied | Vector offers a [14-day evaluation](https://www.vector.com/en/product/pc-lint-plus/pc-lint-plus-evaluation/). Its [compiler configuration generator](https://help.vector.com/pclintplus/InstallationandConfiguration.html) can extract GCC settings; a real evaluation must supply the cross compiler and PPC flags and validate Tiger parsing. No account or trial was activated. |
| 7 | Frama-C 25.0-beta Manganese | 25/34 units parse; no validated whole-application Eva result | With `-machdep ppc_32 -c11`, nine units fail parsing/typing. Whole-program `-eva` stops at an unsupported `_Generic` expression in `bundled_cjson.c:939`. The PPC model also needs Darwin library/variadic and machine-model review; parsed files are not counted as verified coverage. |
| 8 | SonarQube CFamily | Not run; no project scanner/server/license configuration supplied | C/C++ analysis is listed with [Developer Edition and above](https://www.sonarsource.com/products/sonarqube/downloads/). The [prerequisites](https://docs.sonarsource.com/sonarqube-server/analyzing-source-code/languages/c-family/prerequisites) support GCC compilation databases and require access to the build's original paths. A future trial needs the Tiger SDK/compiler paths and an appropriate server/license; PPC/Tiger compatibility is untested. |
| 9 | Splint 3.1.2 | 0/34 units parse | The target-preprocessed declarations stop at GCC's `__builtin_va_list` in `stdarg.h:40`. Normalizing GNU line markers to standard `#line` form removes an incidental input-format blocker but does not solve the language/type incompatibility. No host ABI result is presented as a PPC result. |

### Clang target adaptation

Modern Clang's Darwin driver rejects this PPC target even though its frontend supports PPC32. The standalone analyzer therefore invokes `clang -cc1 -triple powerpc-apple-darwin8 -target-cpu 750` directly. Clang-Tidy uses a generic PPC driver invocation with `-Xclang -triple -Xclang powerpc-apple-darwin8` to select that actual frontend target. All includes are explicit Tiger/Clang resource paths; host libc headers are excluded. Missing Darwin platform predefines come from the pinned GCC's macro output, and the driver is explicitly set to signed char.

Both frontend configurations are audited against GCC's type widths, big-endian order, char signedness, Apple/Mach selectors and deployment macro `1040`. The driver adapter also passes `checks/layout.c` and the recovered structure assertions without disabling them. The audited macro output and commands are under each tool's artifact directory. These checks establish the exercised analysis layout; they do not establish a modern Clang-generated Tiger executable or complete equivalence between the compiler models.

### Extra-tool triage

Flawfinder's 21 highest-level hits include wrappers using `vsnprintf`/`vfprintf`, four `strcpy` calls in allocation-based JSON printing, intentional program execution, and a false lexical match on the parameter named `system` in `detect_macos`. Their presence does not prove a format-string vulnerability or buffer overflow. The search shell calls merit attention because their argument construction was traced, not just because `popen` appears. The broad Clang unsafe-API recommendations are also poor repair prescriptions for Tiger, where Annex K replacements are not the project's established interfaces.

Frama-C's nine failing units are bundled JSON, configuration, conversation state, documentation RAG, model profiles, Ollama, TUI, tools, and workers. The raw logs distinguish unsupported `_Generic` expressions from other typing/normalization failures. The whole-program abort and missing Darwin library/variadic models prevent any claim of a proof or a clean Eva scan. Splint's parsing failure similarly yields no application findings.

## Recommendation after the runs

Keep GCC and Cppcheck as complementary report generators, with a narrow Clang-Tidy pass as the most useful next addition. The standalone Clang analyzer supplies corroboration, but its checks overlap those already exposed through Clang-Tidy. Flawfinder is useful for targeted shell/API review rather than a zero-warning gate. The packaged Frama-C and Splint need substantial compatibility work; commercial tools remain unevaluated until a licensed environment is available.

Prioritize the cache destination calculation and unsafe search shell construction, followed by the subobject traversals, path trimming and provider prefix. GCC's 26 limited units prevent a clean-coverage claim; Cppcheck/Clang completion still leaves model blind spots. Application repairs require a separate change with behavioral verification.
