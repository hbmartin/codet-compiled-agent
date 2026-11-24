# Incomplete validation attempts

Strict campaign acceptance still requires every required check to pass. An offline
integrity audit does not change that policy. Historical bundles and helper scripts
retain their original contents and limitations.

When delivery is explicitly authorized with unavailable measurements, the maintained
assembler accepts only captured quiet-window failures in the measurement checks and
their blocked dependent benchmark check. Other required failures cannot be published
as an authorized incomplete attempt. Actual outcomes, logs, source/input identities,
and zero Release samples remain visible.

Use the validated checkout and its original capture directory, with Python 3.12+:

```sh
python3.12 scripts/evidence_attempt.py \
  --root "$PWD" \
  --campaign /absolute/path/to/frozen-campaign \
  --output docs/validation/queued-input-tiger-recovery-followup/RUN-ID \
  --unavailable-reason "Delivery authorized with benchmarks explicitly unavailable"
python3.12 scripts/evidence_attempt_audit.py \
  docs/validation/queued-input-tiger-recovery-followup/RUN-ID
```

The destination must not exist. Assembly stages a sibling directory, binds its
assembler/auditor helpers to validation fingerprints, verifies the staged inventory,
rechecks inputs, and publishes atomically. It reads no fixed temporary helper paths.
Inside a published bundle, `verify-attempt.py BUNDLE` runs its frozen offline auditor
after relocation; exit zero establishes integrity only, with `eligible_current=false`.
A failed stage never replaces an existing publication. The existing strict bundle
publisher remains the path for a fully successful campaign.

Tiger package determinism is scoped to repeated packaging of one frozen verified
build. Complete audit records retain original paths and configuration. No cross-
checkout byte-reproducibility or native Tiger runtime acceptance is implied.

Campaign build locations use a conservative portable namespace: each path component
is Unicode-NFC normalized and case-folded for collision, overlap, reserved-name,
and control-prefix checks on every host. Case-sensitive hosts therefore reject
some locally distinct names intentionally. Submitted and resolved paths are both
checked before tool probes or campaign creation; actual I/O retains the selected
path spelling and resolved-containment rules. This is not an exact emulation of
all APFS or Tiger HFS+ filename rules.

Historical carrier extraction owns a unique sibling staging directory until its
full audit succeeds and publication is admitted exclusively. Cooperative SIGTERM
and Ctrl-C unwind that staging and restore signal handlers. No automatic stale-tree
sweep runs. SIGKILL, host failure, or an actual cleanup failure can leave staging;
inspect ownership and stop relevant operations before any manual deletion. A
secondary cleanup failure remains attached to the original error rather than
replacing it. Historical integrity never grants current campaign eligibility.

Historical extraction publication is a commit point. Before admission, cooperative
interruption unwinds only the invocation's staging and preserves the original
failure. Signals during exclusive rename and subsequent cleanup are recorded in
the committed result; a completed destination is not removed or reported as an
ordinary interrupted extraction. Genuine post-publication cleanup errors retain
their exception and carry the committed receipt. Existing destinations remain
protected, and no stale-tree reclamation runs.

The relocated offline-audit driver guards selected Python APIs, not every possible
checkout access or process launch. Its file guards cover `builtins.open`, `io.open`,
`os.open`, `os.stat`, and `os.lstat`; its process guards cover the listed `subprocess`
entry points. Cached pathlib metadata accessors (notably on Python 3.9), directory
listing, and direct `os` process primitives are outside that coverage. Historical
receipts' universal prevention booleans overstate this scope; their archived bytes
remain unchanged. New receipts enumerate their APIs and interpreter and make no
OS-sandbox claim. Relocated inventory/provenance verification remains a separate
integrity result, never evidence of current campaign acceptance by itself.

Layout/extraction workload counter contracts version 2 distinguish operations from
files: layout records zero files plus observed successful resolutions and returned
locations; extraction records its actual expanded files/bytes and operation count.
Version-1 workload observations retain their original interpretation only during
explicit historical auditing. Campaign evidence stays at v5. Live extraction
measurements require one declared carrier descriptor shared by both variants and
bound to archived configuration and carrier inputs.


The offline audit driver resolves forbidden checkout roots before relocation and
uses filesystem ancestor identities to detect case and Darwin firmlink aliases.
No-follow metadata operations inspect the final link rather than its referent.
Selected API guards are installed before verifier imports. Tooling comes from the
bundle's `verifier_tooling` directory, or verified archived sources when absent;
bundle-root copies and checkout module caches cannot shadow it. Receipts record
the selected entrypoint digest. Previously cached aliases, cached pathlib accessors,
directory listings, direct OS process primitives, and descriptor-only access remain
outside coverage; this is not a sandbox. Ambiguous relative `dir_fd` lookups are
rejected rather than treated as clean. Import failures restore installed guards.

Before any archived code is imported, the driver hashes every relocated bundle file
with its own `hashlib` and requires exact agreement with `inventory.json`, whose digest
the receipt records. That binds the entrypoint, sibling tooling modules, `inputs.json`,
and the archived inputs that fallback tooling is reconstructed from. Symlinks are
preserved during relocation and rejected. Only the selected tooling directory is
prepended to the import path; the bundle root is never importable. No-follow
operations also compare the final component's own identity, so a case or firmlink
spelling of an original root is denied. Relative `dir_fd` lookups are denied as
`PermissionError`, recorded, and fail the audit even if a verifier catches the error;
stdlib helpers that need them, such as `shutil.rmtree` on nested directories, cannot
run inside the guard.
Use `python3 scripts/evidence_offline_audit.py BUNDLE RECEIPT ORIGINAL_ROOT...`.
The optional `--expect-unmet '{"benchmarks":"failed"}'` checks an explicit campaign
expectation; otherwise the underlying auditor validates and reports the disposition.
Incomplete integrity verification never grants current campaign eligibility.

Committed historical publication ownership extends through cleanup and operation
scope teardown. Cooperative interruptions remain observations on the committed
receipt. Genuine cleanup and operation failures retain their identity, original
cause, and secondary error observations even if teardown is interrupted. Only
pure committed interruptions recover as success. Measurements reject interruption
observations before writing a sample and retain the receipt in failure diagnostics.
An outer timeout does not become a successful sample. A preserved failure also keeps
the implicit context it had when first registered; a later teardown or interrupt
is recorded as a secondary observation, never as the failure's context.

Signal-fixture cleanup treats a vanished leader (`ENOENT`/`ESRCH`) like any other
ambiguous inspection: it returns only after positively observing an empty process
group, and otherwise fails. Ctrl-C and `SystemExit` during fixture cleanup still
stop the run after the leader group and streams are cleaned; they are never
recorded as cleanup failures.

Extraction workload contract 4 measures the complete verified operation: carrier
admission, extraction and auditing, final carrier verification, and owned cleanup.
The harness calls each variant's native hasher, including its buffer strategy, and
observes bytes read through transparent stream forwarding. Successful native hash
calls and actual read bytes are separate from expanded files/bytes. Three complete
archive hash reads are required. Instrumentation overhead is inside timing and
tracing; these are instrumented verified-operation measurements, not isolated or
uninstrumented hashing costs. Version-1/2/3 extraction observations retain their
original meaning only in explicit historical auditing. Earlier contract 3 used a
shared harness hash loop and must not be described as native-hasher measurements.
Each contract owns one hash-byte counter: contract 4 rows must not carry
`archive_hash_bytes`, and contract 3 rows must not carry `archive_hash_read_bytes`.
Historical samples and receipts are unchanged. Evidence containers stay at v5;
live tooling observations require integer metric version 2. No timing threshold
or general performance claim follows from these deterministic work checks.
