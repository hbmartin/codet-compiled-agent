# GUI configuration recovery and explicit historical auditing

The complete evidence-v5 run is [9d72ab94e10e429abb31ee48a5574b21](9d72ab94e10e429abb31ee48a5574b21/manifest.json).
All **42 required checks passed**. Both analyzer observation checks retain their
actual exit 1; matching known findings are not relabeled as successful checks.

- Baseline: `b82098d1a05f85ffe78e9e4898a03508127a6f8d`.
- Frozen implementation, tests and producers: `cfe920fc1c553899d32372e22a87f851ad764ec1`.
- Campaign: `04a2f865315d46d189e145e26d6327a6`.
- The final evidence commit adds documentation and transport only. Production and
  validation fingerprints are rechecked against the frozen capture.

## Changes and reproductions

GUI-equivalent edits retaining the cache's PATH type and managed help previously
failed as corruption. A nonempty edit now becomes explicit only after private
metadata and both prior commitment records validate independently of the edited
public value. A corrupt, missing or provisional commitment cannot establish that
intent and gets reset/override instructions. Clearing the public value resets
selection even with retained help/type or damaged metadata. Genuine current-format
`-D` type/help overrides retain recovery before parsing replaced records.

Internal selection format 3 is unchanged. The separate witness proves successful
project configuration, not completion of generation. Later generator errors can
retain both records. Missing either record alone fails reuse; removing both,
including all of `CMakeFiles/`, permits coherent provisional rediscovery. Obsolete
owned metadata still requires reset; no migration is introduced.

The current bundle verifier now exposes keyword-only `historical=True` and an
explicit CLI flag. Historical v5 auditing retains inventory, source/archive,
producer, environment, working-directory, sidecar and recomputed-summary checks.
A successful result is `verified=true`, `historical=true`, `eligible_current=false`.
It never satisfies current publication or campaign acceptance. Live preparation
requires integer metric version 2; changed preflight controls require 256 advisory
checks. Version-2 baseline/historical controls admit only the original 128/256
values. Version-1 observations retain their original interpretation without
inventing absent counters. Allocation and wrapper controls remain enforced.

The captured `historical-audit` check expands and verifies the unchanged historical
v5 carrier from `configuration-input-preflight-followup/68a48c6fee9743c08d6a5e9f21d0aeef`.
It checks all 477 files / 383,683,534 expanded bytes. Strict current auditing rejects
its version-1 metrics; explicit historical auditing succeeds with current
eligibility false. This is compatibility evidence, not validation of changed code
by an older campaign.

Replay templates now declare the Make build directory. Older four-build templates
receive the recorded `make-preparation` default. Historical adapters are generated
from one source snapshot, atomically replaced, and preserve existing permissions;
new files respect the process umask. The explicit adapter utility remains usable.
Ordinary benchmark builds still compile the shared production helper directly.
The shared facade now has an executed request-bound guard regression and mutant.
Public runtime behavior, limits, raw bytes and storage are unchanged; the hashed
sysroot builder is untouched. Evidence v5, Tiger provenance 3 and sysroot 2 remain.

## Captured validation

| Check | Result |
| --- | --- |
| Debug CTest | 13/13 passed |
| ASan/UBSan CTest | 13/13 passed; Darwin unit cases isolated |
| TSan focused repetition | 140 cases, 20 repetitions passed |
| Regular focused repetition | 140 cases, 20 repetitions passed |
| Verified Python 3.9.6 tooling | 159 tests passed |
| Python 3.9.6 integration | 138 tests passed |
| Python attempt/provenance/packaging fixtures | Passed |
| Real Make preparation build, eight-workload smoke, unchanged build | Passed |
| Pinned lint, Semgrep and runner checks | Passed |
| Host cross-selection | Build and 31 host cases passed; not native PPC |
| Python/CMake negative controls | 70 passed |
| Native/CLI negative controls | 62 passed |
| Production-site Semgrep controls | 2 passed |
| Historical v5 compatibility audit | Passed; historical eligibility remains false |
| Static comparison | Passed, all 25 application translation units |
| Fresh Release measurements | Passed, 1,032 raw / 860 measured samples |

Both variants retain 147 Clang-Tidy findings, zero Cppcheck findings and one Clang
analyzer finding. Their actual analyzer exits are observations. Strict comparison
found no added/removed findings and required no relocations. Controls require a
passing baseline and the intended executed assertion. Skips, compilation,
discovery, launch and timeout errors do not count as mutation detection.

Reproductions cover GUI values/clearing, ambiguous private corruption, receipt and
witness loss/mismatch, scoped values, explicit overrides, Make/Ninja regeneration,
configuration/generation failures, historical/current audit separation, corrupt
archives/producers/sidecars/summaries, metric types/counts, replay defaults,
adapter permissions/atomicity and request-bound preparation.

## Measurements

The immutable baseline and changed variants use identical declared harness/support
snapshots and matching compiler settings. Source, compiler/configuration,
executable, library, adapter and compile/link identities are captured. There is
one warm-up plus five alternating measured runs per variant in each of two sets.
Quiet-process checks and deterministic positive controls passed. All 43 workloads,
1,032 raw samples, 860 measured samples and recomputed summaries are retained.

Every changed preflight sample records 256 advisory checks. All changed saturated
samples record zero payload-sized allocations, and every accepted changed CLI
preparation has one wrapper. The baseline already has those same counter results;
this follow-up makes no new allocation-reduction claim.

Selected median latency differences, changed relative to baseline:

| Workload | Set 1 | Set 2 |
| --- | ---: | ---: |
| Preflight checks | +11.11% | -3.22% |
| CLI preparation | -8.05% | -2.15% |
| Configuration selection | -0.51% | -3.51% |
| Historical sample auditing | -0.09% | +0.82% |
| Amplified diagnostics | -0.88% | -1.00% |
| Small exports | -2.34% | -0.64% |

These are observations, with no timing threshold or general speed claim.
`all_measured_work_equal=false`; scoped counter/memory differences remain in the
verified data. Historical-sample timing covers recomputing 24 version-1 rows 32
times, not full disk-bundle auditing. Traced retained blocks/bytes and peak bytes
are memory observations, not total allocation events or kernel metadata syscalls.

## Replay and offline auditing

The linked manifest is the exact concrete capture command set. Use a new campaign
folder and explicit local tool overrides when replaying on another machine; do
not rewrite this historical manifest or reuse an existing campaign directory.
For example, with the pinned tools already provisioned:

```sh
python3 scripts/evidence_campaign.py prepare --root /absolute/repository \
  --folder /absolute/fresh-campaign --baseline b82098d1a05f85ffe78e9e4898a03508127a6f8d \
  --check-python /absolute/pinned-python --analysis-python /absolute/pinned-python \
  --python39 /absolute/python3.9 --clang-tidy /absolute/clang-tidy \
  --cppcheck /absolute/cppcheck
/absolute/pinned-python scripts/validation_evidence.py capture \
  /absolute/fresh-campaign/manifest.json /absolute/fresh-campaign/capture
/absolute/pinned-python scripts/validation_evidence.py bundle \
  /absolute/fresh-campaign/manifest.json /absolute/fresh-campaign/capture \
  --output /absolute/new-immutable-bundle
```

The complete expanded bundle is transported losslessly in
[`audit-bundle.tar.gz`](9d72ab94e10e429abb31ee48a5574b21/audit-bundle.tar.gz). Extract it into a fresh directory;
it expands beneath `9d72ab94e10e429abb31ee48a5574b21`. Then use:

```sh
python3 scripts/validation_evidence.py verify-bundle /absolute/expanded-bundle
python3 scripts/validation_evidence.py verify-bundle /absolute/older-v5-bundle --historical
```

The second command exits zero only for verified historical integrity and still
reports `eligible_current=false`. Default auditing and publication remain strict.
Older evidence container formats retain their reported limitations; the historical
metric option does not backfill missing provenance.

[Offline audit](9d72ab94e10e429abb31ee48a5574b21/offline-audit.json) relocated the final bundle under Python
3.9.6, loaded archived verifier code, forbade original-checkout I/O and process
launches, and returned current eligibility true for this new run. It verified all
44 captured records and 14,553 audit inputs, then rejected a corrupted archived
input with `Audit inventory mismatch`. It did not rebuild or rerun binaries.

[Transport metadata](9d72ab94e10e429abb31ee48a5574b21/transport.json) retains the exact helper source, arguments,
inventory identity and round-trip proof. It preserves 490 files and
396,507,447 expanded bytes in 18,971,445 compressed bytes. Archive SHA-256:
`f7002294af73961b27d45d7012657833071d8a56a59819485c452f494aa8775f`.
Every decompressed member was checked against the complete published inventory.
Reports remain expanded inside the carrier; no report compaction or reconstructed
outcomes were used. Plain manifest/sample/median sidecars are previews of the same
archived bytes. The legacy transport preview has no top-level sample count; the
authoritative 1,032 count is in the verified measurement artifact and CSVs.
Executables, SDKs and dependency installations retain identities and references
rather than being bundled.

## Limits

CMake 4.4.4 and native Darwin were exercised. CMake 3.20 runtime was not exercised; available tool resolution selected 4.4.4. Docker, Podman and the PPC compiler were unavailable; Ubuntu
Unicode runtime and native Tiger acceptance remain unavailable. Host fixtures and
cross-selection do not establish target acceptance. No older CI run is substituted
for this frozen source. Historical artifacts and the original checkout are
preserved.
