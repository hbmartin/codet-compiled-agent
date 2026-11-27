# Reliability repair verification

The series starts at `097ff937c5746cb4892cc677f45a2a449ec91646`. Application and packaging changes end at `4a2de2821c6890017aef0fbfe70398c0dc264a46`. Three subsequent test-only commits resolve GCC diagnostics in conditional GoogleTest assertions and the preparation benchmark. The GCC-compatible revision is `7a185c066d18b3e4001e59171a6053aec3cb7e49`. Two fixture commits culminate in `d3f6940a7d8a5bb38c5039042530d82fc652298d`. They synchronize Ctrl-D admission and verify cleanup of a known child when process-table queries stall, under the original deadlines. An injected stall left a real test child running on the earlier fixture and was cleaned up in 0.4 seconds with the fix.

## Behavior checked against the original main

| Area | Original behavior | Result with the repair |
|---|---|---|
| Patch conflicts | An Add or Move overwrote a file created during approval. | Exclusive destination publication preserves the competing bytes and permissions. The crash and rollback fixtures check which named copies survive each boundary. |
| macOS file metadata | The real CLI created a `0644` file under umask `0077`. Editing a file removed its resource fork and test xattr. | The same CLI fixture produced mode `0600` and preserved both values. Native tests also check ACLs, metadata-copy failure, first-visible modes and rollback. |
| Conversation change | Both terminal fixtures switched immediately while a shell job remained active. | Both require fresh confirmation. The regression checks the database session count, child PID, decline, interrupt, acceptance and a child exiting during the question. |
| Credentials and configuration | The baseline CLI regressions failed for interrupted credential input and invalid numeric settings. | The fixed CLI preserves the prior credential on read failure or interruption. Cross-process tests cover serialized key, import and token rotation writes. Configuration tests reject invalid types and out-of-range original values. |
| State recovery | Baseline fixtures reproduced migration descriptor exhaustion, failed or interrupted fork publication, and export failure after corruption confined to the provider cache. | The state CLI suite checks migration under a low descriptor limit, restoration from a real pre-v8 backup, fork recovery and read-only export of intact history. |
| Provider streams | Responses omitted refusal text and accepted premature completion. For four times as many Anthropic fragments, measured allocations rose from 29,768,501 to 458,789,429 bytes. | The local provider fixtures check streamed and final refusal, protocol-specific completion and exact signed replay. The same allocation probe measured 1,687,853 and 6,729,773 bytes. |
| Package publication | Two successful overlapping publishers left writer B’s archive and writer A’s checksum. | The tests verify a consistent pinned pair across overlapping writers, process exits on either side of the switch and interrupted adoption of existing flat files. |
| Developer image | Starting the analyzer from the original Docker inputs failed first on `process_supervision` and then on `verified_sources`. | The host fixture and an offline BuildKit smoke test both ran the analyzer’s `--help` using the declared image inputs. |

## Execution results

Host fixtures use disposable files, dummy credentials and loopback providers. macOS verification used AppleClang 21.0.0.21000401, CMake 4.5.1, Python 3.12.16 and OpenSSL 3.5.9 on macOS 27.0.1.

| Gate | Outcome |
|---|---|
| macOS native CTest | All 15 groups passed on the final fixture revision in 242.04 s, including unit, process-publication faults, CLI integration, packaging and validation evidence. The 143-case CLI group took 86.91 s. Packaging took 49.70 s and validation evidence took 33.70 s under their 180 s limits. Earlier runs under heavier concurrent builds hit deadlines or exposed the fixture races described above. |
| Ubuntu ARM64 native CTest | GNU 13.3.0 on Linux 7.0.0-34-generic. All 15 groups passed in 172.56 s, including CLI integration in 64.07 s. The earlier amd64-translated run hit timeouts and signal failures. |
| macOS ASan/UBSan | All four final CTest gates passed together in 138.13 s under their normal budgets. The gates cover native units, publication faults, provider-stream allocations and state recovery. All 18 focused patch regressions passed. A separate final unit run passed 731 tests with two opt-in skips and one disabled test. Earlier runs under concurrent builds exceeded the 300 s unit wrapper. |
| Lint, Semgrep and Cppcheck | The repository lint gate passed. Semgrep 1.177.0 ran 32 rules against 72 files with no findings or scan errors. The scan used a 90 s rule timeout. Pinned Cppcheck 2.22.0 checked all 26 production translation units, reported zero first-party findings and passed all 15 controls. |
| Clang static analyzer | Analyzed all 26 production translation units. It reported the existing `core.DivideZero` diagnostic in `context.cpp`. A separate analysis of the final patch publication implementation reported no diagnostics. |
| Clang-tidy | LLVM 23.1.6 reported 168 warnings. The same scanner reported 147 on the baseline. Individual matching found 27 added and six removed. The additions were 19 signed bitwise operations, five constant widenings, two empty exception handlers and one vector capacity suggestion. This gate remains nonzero. |
| PowerPC Tiger Release | Built the seven pinned dependencies into a format-2 sysroot, then compiled and linked `codet`, `codet_tests` and `codet_persistence_bench` with GCC 14.2 for PPC/G3. The official static verifier passed. Build ID `8361944174584bcf96a9e18773ac0858` binds the binaries to the final source revision. |
| TigerPorts and portable packages | Both archive commands passed. Independent checks resolved and verified the paired generations, the three executable digests in each archive, the build provenance, and the portable package’s eight libraries and CA digest. The archive hashes and byte counts are in the [package verification record](reliability-repair-archives.json). |

## Remaining target acceptance

An interrupted Move can leave both the original and edited copy. At the tested revision `d3f6940`, ordinary in-place Update had a window between its final identity check and atomic replacement if an independent writer concurrently replaced that path. The subsequent fix and its recovery behavior are described in the [tool contract](../CODEX-TOOLS.md#workspace-patch-publication). The crash fixtures terminate processes at known publication boundaries. A physical power interruption on Tiger/HFS+ and execution on a 512 MB G4 require the target hardware. The [target acceptance checklist](../TIGER-ACCEPTANCE.md) covers edited moves, permissions, resource forks, ACLs, session changes with live jobs and migration of copied historical state. Live provider accounts were outside the fixture runs.
